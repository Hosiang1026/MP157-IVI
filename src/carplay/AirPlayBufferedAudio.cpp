#include "AirPlayBufferedAudio.hpp"

#include "AirPlayCrypto.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>

#include <cstring>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {
constexpr int kRtpHeader = 12;
constexpr int kTag = 16;
constexpr int kNonce = 8;
constexpr qint64 kU32 = 0xffffffffLL;

#ifdef Q_OS_WIN
constexpr int kAvCodecIdAac = 86018;

struct AVPacket;
struct AVFrame;
struct AVCodec;
struct AVCodecContext;
struct AVPacketLite {
    void *buf;
    qint64 pts;
    qint64 dts;
    uint8_t *data;
    int size;
};
struct AVFrameLite {
    uint8_t *data[8];
    int linesize[8];
    uint8_t **extended_data;
    int width;
    int height;
    int nb_samples;
    int format;
};

struct AacDecoder {
    HMODULE avcodec = nullptr;
    HMODULE avutil = nullptr;
    using Find = const AVCodec *(*)(int);
    using Alloc = AVCodecContext *(*)(const AVCodec *);
    using Open = int (*)(AVCodecContext *, const AVCodec *, void *);
    using FreeCtx = void (*)(AVCodecContext **);
    using Send = int (*)(AVCodecContext *, const AVPacket *);
    using Recv = int (*)(AVCodecContext *, AVFrame *);
    using PktAlloc = AVPacket *(*)();
    using PktFree = void (*)(AVPacket **);
    using PktUnref = void (*)(AVPacket *);
    using NewPkt = int (*)(AVPacket *, int);
    using FrAlloc = AVFrame *(*)();
    using FrFree = void (*)(AVFrame **);
    using FrUnref = void (*)(AVFrame *);
    Find find = nullptr;
    Alloc alloc = nullptr;
    Open open2 = nullptr;
    FreeCtx freeCtx = nullptr;
    Send send = nullptr;
    Recv recv = nullptr;
    PktAlloc pktAlloc = nullptr;
    PktFree pktFree = nullptr;
    PktUnref pktUnref = nullptr;
    NewPkt newPkt = nullptr;
    FrAlloc frAlloc = nullptr;
    FrFree frFree = nullptr;
    FrUnref frUnref = nullptr;
    AVCodecContext *ctx = nullptr;
    AVPacket *pkt = nullptr;
    AVFrame *frame = nullptr;

    HMODULE load(const wchar_t *name)
    {
        const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromWCharArray(name));
        if (const HMODULE mod = LoadLibraryW(reinterpret_cast<LPCWSTR>(path.utf16())))
            return mod;
        return LoadLibraryW(name);
    }

    bool init()
    {
        if (ctx)
            return true;
        SetDllDirectoryW(reinterpret_cast<LPCWSTR>(QCoreApplication::applicationDirPath().utf16()));
        avutil = load(L"avutil-60.dll");
        avcodec = load(L"avcodec-62.dll");
        if (!avcodec || !avutil)
            return false;
        find = reinterpret_cast<Find>(GetProcAddress(avcodec, "avcodec_find_decoder"));
        alloc = reinterpret_cast<Alloc>(GetProcAddress(avcodec, "avcodec_alloc_context3"));
        open2 = reinterpret_cast<Open>(GetProcAddress(avcodec, "avcodec_open2"));
        freeCtx = reinterpret_cast<FreeCtx>(GetProcAddress(avcodec, "avcodec_free_context"));
        send = reinterpret_cast<Send>(GetProcAddress(avcodec, "avcodec_send_packet"));
        recv = reinterpret_cast<Recv>(GetProcAddress(avcodec, "avcodec_receive_frame"));
        pktAlloc = reinterpret_cast<PktAlloc>(GetProcAddress(avcodec, "av_packet_alloc"));
        pktFree = reinterpret_cast<PktFree>(GetProcAddress(avcodec, "av_packet_free"));
        pktUnref = reinterpret_cast<PktUnref>(GetProcAddress(avcodec, "av_packet_unref"));
        newPkt = reinterpret_cast<NewPkt>(GetProcAddress(avcodec, "av_new_packet"));
        frAlloc = reinterpret_cast<FrAlloc>(GetProcAddress(avutil, "av_frame_alloc"));
        frFree = reinterpret_cast<FrFree>(GetProcAddress(avutil, "av_frame_free"));
        frUnref = reinterpret_cast<FrUnref>(GetProcAddress(avutil, "av_frame_unref"));
        if (!find || !alloc || !open2 || !freeCtx || !send || !recv || !pktAlloc || !pktFree || !pktUnref
            || !newPkt || !frAlloc || !frFree || !frUnref)
            return false;
        const AVCodec *codec = find(kAvCodecIdAac);
        if (!codec)
            return false;
        ctx = alloc(codec);
        if (!ctx || open2(ctx, codec, nullptr) < 0)
            return false;
        pkt = pktAlloc();
        frame = frAlloc();
        return pkt && frame;
    }

    void close()
    {
        if (frame && frFree)
            frFree(&frame);
        if (pkt && pktFree)
            pktFree(&pkt);
        if (ctx && freeCtx)
            freeCtx(&ctx);
        frame = nullptr;
        pkt = nullptr;
        ctx = nullptr;
    }

    QByteArray decode(const QByteArray &aac)
    {
        if (!init() || aac.isEmpty())
            return {};
        pktUnref(pkt);
        if (newPkt(pkt, aac.size()) < 0)
            return {};
        auto *lite = reinterpret_cast<AVPacketLite *>(pkt);
        std::memcpy(lite->data, aac.constData(), size_t(aac.size()));
        if (send(ctx, pkt) < 0)
            return {};
        QByteArray pcm;
        for (int i = 0; i < 4; ++i) {
            frUnref(frame);
            const int err = recv(ctx, frame);
            if (err < 0)
                break;
            auto *fr = reinterpret_cast<AVFrameLite *>(frame);
            if (fr->nb_samples <= 0 || !fr->data[0])
                continue;
            if (fr->format == 1) {
                pcm.append(reinterpret_cast<const char *>(fr->data[0]), fr->nb_samples * 4);
            } else if (fr->format == 6 && fr->data[0] && fr->data[1]) {
                const qint16 *l = reinterpret_cast<const qint16 *>(fr->data[0]);
                const qint16 *r = reinterpret_cast<const qint16 *>(fr->data[1]);
                for (int s = 0; s < fr->nb_samples; ++s) {
                    pcm.append(char(l[s] & 0xff));
                    pcm.append(char((l[s] >> 8) & 0xff));
                    pcm.append(char(r[s] & 0xff));
                    pcm.append(char((r[s] >> 8) & 0xff));
                }
            } else if (fr->format == 8 && fr->data[0] && fr->data[1]) {
                const float *l = reinterpret_cast<const float *>(fr->data[0]);
                const float *r = reinterpret_cast<const float *>(fr->data[1]);
                for (int s = 0; s < fr->nb_samples; ++s) {
                    const int ls = qBound(-32768, int(l[s] * 32767.f), 32767);
                    const int rs = qBound(-32768, int(r[s] * 32767.f), 32767);
                    pcm.append(char(ls & 0xff));
                    pcm.append(char((ls >> 8) & 0xff));
                    pcm.append(char(rs & 0xff));
                    pcm.append(char((rs >> 8) & 0xff));
                }
            }
        }
        return pcm;
    }
};
#endif
} // namespace

AirPlayBufferedAudio::AirPlayBufferedAudio(const QByteArray &key, int sampleRate, int channels,
                                           QObject *parent)
    : QObject(parent)
    , m_key(key)
    , m_sampleRate(sampleRate > 0 ? sampleRate : 48000)
    , m_channels(channels > 0 ? channels : 2)
{
}

AirPlayBufferedAudio::~AirPlayBufferedAudio()
{
    stop();
}

bool AirPlayBufferedAudio::start()
{
    stop();
    m_server = std::make_unique<QTcpServer>(this);
    if (!m_server->listen(QHostAddress::AnyIPv4, 0)) {
        m_server.reset();
        return false;
    }
    m_port = m_server->serverPort();
    connect(m_server.get(), &QTcpServer::newConnection, this, &AirPlayBufferedAudio::onNewConnection);
#ifdef Q_OS_WIN
    m_aacDecoder = new AacDecoder;
#endif
    emit log(QStringLiteral("buffered audio listen tcp=%1 rate=%2").arg(m_port).arg(m_sampleRate));
    return true;
}

void AirPlayBufferedAudio::stop()
{
    if (m_client) {
        m_client->disconnect(this);
        m_client->abort();
        m_client->deleteLater();
        m_client = nullptr;
    }
    if (m_server) {
        m_server->disconnect(this);
        m_server->close();
        m_server.reset();
    }
#ifdef Q_OS_WIN
    if (m_aacDecoder) {
        static_cast<AacDecoder *>(m_aacDecoder)->close();
        delete static_cast<AacDecoder *>(m_aacDecoder);
        m_aacDecoder = nullptr;
    }
#endif
    m_buf.clear();
    m_port = 0;
    m_rate = 0;
    m_anchorRtp = -1;
    m_flushUntil = -1;
}

QVariantMap AirPlayBufferedAudio::makeAnchor(qint64 rtp, int rate)
{
    const qint64 secs = QDateTime::currentSecsSinceEpoch() + 1;
    return QVariantMap{
        {QStringLiteral("rtpTime"), qlonglong(rtp & kU32)},
        {QStringLiteral("networkTimeSecs"), qlonglong(secs)},
        {QStringLiteral("networkTimeFrac"), qlonglong(0)},
        {QStringLiteral("rate"), rate},
    };
}

QVariantMap AirPlayBufferedAudio::setRate(qint64 rtpTime, int rate)
{
    m_rate = rate > 0 ? 1 : 0;
    if (m_rate > 0) {
        if (rtpTime >= 0)
            m_anchorRtp = rtpTime & kU32;
        else if (m_anchorRtp < 0)
            m_anchorRtp = 0;
        m_flushUntil = m_anchorRtp;
        emit log(QStringLiteral("buffered SETRATE rate=1 rtp=%1").arg(m_anchorRtp));
    } else {
        emit log(QStringLiteral("buffered SETRATE rate=0"));
    }
    return makeAnchor(m_anchorRtp < 0 ? 0 : m_anchorRtp, m_rate);
}

QVariantMap AirPlayBufferedAudio::anchor() const
{
    if (m_anchorRtp < 0)
        return {};
    return makeAnchor(m_anchorRtp, m_rate);
}

void AirPlayBufferedAudio::flush(qint64 untilTs)
{
    if (untilTs >= 0)
        m_flushUntil = untilTs & kU32;
    emit log(QStringLiteral("buffered FLUSH until=%1").arg(m_flushUntil));
}

void AirPlayBufferedAudio::onNewConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket *sock = m_server->nextPendingConnection();
        if (m_client) {
            sock->abort();
            sock->deleteLater();
            continue;
        }
        m_client = sock;
        m_client->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        connect(m_client, &QTcpSocket::readyRead, this, &AirPlayBufferedAudio::onClientReadyRead);
        connect(m_client, &QTcpSocket::disconnected, this, &AirPlayBufferedAudio::onClientDisconnected);
        emit log(QStringLiteral("buffered audio connected %1").arg(m_client->peerAddress().toString()));
        m_server->close();
    }
}

void AirPlayBufferedAudio::onClientDisconnected()
{
    emit log(QStringLiteral("buffered audio disconnected"));
    if (m_client) {
        m_client->deleteLater();
        m_client = nullptr;
    }
    m_buf.clear();
}

void AirPlayBufferedAudio::onClientReadyRead()
{
    if (!m_client)
        return;
    m_buf.append(m_client->readAll());
    processBuffer();
}

bool AirPlayBufferedAudio::openFrame(const QByteArray &body, QByteArray *aacOut, qint64 *tsOut)
{
    if (body.size() < kRtpHeader + kTag + kNonce)
        return false;
    if (quint8(body[0]) != 0x80)
        return false;
    const int sealedEnd = body.size() - kNonce;
    QByteArray nonce(12, 0);
    nonce.replace(4, 8, body.mid(sealedEnd, kNonce));
    const QByteArray aad = body.mid(4, 8);
    const QByteArray sealed = body.mid(kRtpHeader, sealedEnd - kRtpHeader);
    const QByteArray payload = AirPlayCrypto::chachaOpen(m_key, nonce, sealed, aad);
    if (payload.isNull())
        return false;
    *aacOut = payload;
    *tsOut = (qint64(quint8(body[4])) << 24) | (qint64(quint8(body[5])) << 16)
        | (qint64(quint8(body[6])) << 8) | qint64(quint8(body[7]));
    return true;
}

QByteArray AirPlayBufferedAudio::decodeAac(const QByteArray &aac)
{
#ifdef Q_OS_WIN
    auto *dec = static_cast<AacDecoder *>(m_aacDecoder);
    if (!dec)
        return {};
    return dec->decode(aac);
#else
    Q_UNUSED(aac);
    return {};
#endif
}

void AirPlayBufferedAudio::processBuffer()
{
    while (m_buf.size() >= 2) {
        const int length = (quint8(m_buf[0]) << 8) | quint8(m_buf[1]);
        if (length < 2 + kRtpHeader + kTag + kNonce) {
            emit log(QStringLiteral("buffered bad length=%1").arg(length));
            m_buf.clear();
            return;
        }
        if (m_buf.size() < length)
            return;
        const QByteArray body = m_buf.mid(2, length - 2);
        m_buf.remove(0, length);
        QByteArray aac;
        qint64 ts = 0;
        if (!openFrame(body, &aac, &ts)) {
            ++m_fail;
            if (m_fail <= 3)
                emit log(QStringLiteral("buffered decrypt fail #%1").arg(m_fail));
            continue;
        }
        if (m_flushUntil >= 0) {
            const qint64 delta = (ts - m_flushUntil) & kU32;
            if (delta >= 0x80000000LL)
                continue;
        }
        ++m_ok;
        if (m_ok <= 3 || (m_ok % 200) == 0)
            emit log(QStringLiteral("buffered pkt #%1 aac=%2 ts=%3 rate=%4")
                         .arg(m_ok)
                         .arg(aac.size())
                         .arg(ts)
                         .arg(m_rate));
        if (m_rate <= 0)
            continue;
        const QByteArray pcm = decodeAac(aac);
        if (!pcm.isEmpty())
            emit pcmReady(pcm);
    }
}

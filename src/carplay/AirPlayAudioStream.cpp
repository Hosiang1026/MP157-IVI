#include "AirPlayAudioStream.hpp"

#include "AirPlayCrypto.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QNetworkDatagram>

#include <cstring>

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
#define IVI_FFMPEG_DYN 1
#endif

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(Q_OS_LINUX)
#include <dlfcn.h>
#endif

namespace {
constexpr int kRtpHeader = 12;
constexpr int kTag = 16;
constexpr int kNonce = 8;

#ifdef IVI_FFMPEG_DYN
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
    int key_frame;
    int pict_or_sample;
    int sample_rate;
};

#ifdef Q_OS_WIN
using LibHandle = HMODULE;
static void *sym(LibHandle h, const char *n) { return reinterpret_cast<void *>(GetProcAddress(h, n)); }
#else
using LibHandle = void *;
static void *sym(LibHandle h, const char *n) { return dlsym(h, n); }
#endif

struct AacDecoder {
    LibHandle avcodec = nullptr;
    LibHandle avutil = nullptr;
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

#ifdef Q_OS_WIN
    LibHandle load(const wchar_t *name)
    {
        const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromWCharArray(name));
        if (const LibHandle mod = LoadLibraryW(reinterpret_cast<LPCWSTR>(path.utf16())))
            return mod;
        return LoadLibraryW(name);
    }
#else
    LibHandle load(const char *name)
    {
        const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromUtf8(name));
        if (QFileInfo::exists(path)) {
            if (LibHandle mod = dlopen(path.toUtf8().constData(), RTLD_NOW | RTLD_GLOBAL))
                return mod;
        }
        return dlopen(name, RTLD_NOW | RTLD_GLOBAL);
    }
#endif

    bool init()
    {
        if (ctx)
            return true;
#ifdef Q_OS_WIN
        SetDllDirectoryW(reinterpret_cast<LPCWSTR>(QCoreApplication::applicationDirPath().utf16()));
        avutil = load(L"avutil-60.dll");
        avcodec = load(L"avcodec-62.dll");
#else
        static const char *kUtil[] = {"libavutil.so", "libavutil.so.59", "libavutil.so.58", "libavutil.so.57",
                                      nullptr};
        static const char *kCodec[] = {"libavcodec.so", "libavcodec.so.61", "libavcodec.so.60", "libavcodec.so.59",
                                       "libavcodec.so.58", nullptr};
        for (int i = 0; kUtil[i] && !avutil; ++i)
            avutil = load(kUtil[i]);
        for (int i = 0; kCodec[i] && !avcodec; ++i)
            avcodec = load(kCodec[i]);
#endif
        if (!avcodec || !avutil)
            return false;
        find = reinterpret_cast<Find>(sym(avcodec, "avcodec_find_decoder"));
        alloc = reinterpret_cast<Alloc>(sym(avcodec, "avcodec_alloc_context3"));
        open2 = reinterpret_cast<Open>(sym(avcodec, "avcodec_open2"));
        freeCtx = reinterpret_cast<FreeCtx>(sym(avcodec, "avcodec_free_context"));
        send = reinterpret_cast<Send>(sym(avcodec, "avcodec_send_packet"));
        recv = reinterpret_cast<Recv>(sym(avcodec, "avcodec_receive_frame"));
        pktAlloc = reinterpret_cast<PktAlloc>(sym(avcodec, "av_packet_alloc"));
        pktFree = reinterpret_cast<PktFree>(sym(avcodec, "av_packet_free"));
        pktUnref = reinterpret_cast<PktUnref>(sym(avcodec, "av_packet_unref"));
        newPkt = reinterpret_cast<NewPkt>(sym(avcodec, "av_new_packet"));
        frAlloc = reinterpret_cast<FrAlloc>(sym(avutil, "av_frame_alloc"));
        frFree = reinterpret_cast<FrFree>(sym(avutil, "av_frame_free"));
        frUnref = reinterpret_cast<FrUnref>(sym(avutil, "av_frame_unref"));
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
            if (err == -11)
                break;
            if (err < 0)
                break;
            auto *fr = reinterpret_cast<AVFrameLite *>(frame);
            if (fr->nb_samples <= 0 || !fr->data[0])
                continue;
            // AV_SAMPLE_FMT_FLTP=8, S16=1, S16P=6 — convert float planar roughly if needed
            if (fr->format == 1) {
                pcm.append(reinterpret_cast<const char *>(fr->data[0]), fr->nb_samples * 2 * 2);
            } else if (fr->format == 6 && fr->data[0] && fr->data[1]) {
                pcm.reserve(pcm.size() + fr->nb_samples * 4);
                const qint16 *l = reinterpret_cast<const qint16 *>(fr->data[0]);
                const qint16 *r = reinterpret_cast<const qint16 *>(fr->data[1]);
                for (int s = 0; s < fr->nb_samples; ++s) {
                    const qint16 ls = l[s];
                    const qint16 rs = r[s];
                    pcm.append(char(ls & 0xff));
                    pcm.append(char((ls >> 8) & 0xff));
                    pcm.append(char(rs & 0xff));
                    pcm.append(char((rs >> 8) & 0xff));
                }
            } else if (fr->format == 8 && fr->data[0] && fr->data[1]) {
                pcm.reserve(pcm.size() + fr->nb_samples * 4);
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
}

AirPlayAudioStream::AirPlayAudioStream(const QByteArray &key, int sampleRate, int channels, bool aac,
                                       QObject *parent)
    : QObject(parent)
    , m_key(key)
    , m_sampleRate(sampleRate)
    , m_channels(channels)
    , m_aac(aac)
{
}

AirPlayAudioStream::~AirPlayAudioStream()
{
    stop();
}

bool AirPlayAudioStream::start()
{
    stop();
    m_data = std::make_unique<QUdpSocket>(this);
    m_control = std::make_unique<QUdpSocket>(this);
    if (!m_data->bind(QHostAddress::AnyIPv4, 0) || !m_control->bind(QHostAddress::AnyIPv4, 0)) {
        stop();
        return false;
    }
    m_data->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 512 * 1024);
    m_dataPort = m_data->localPort();
    m_controlPort = m_control->localPort();
    connect(m_data.get(), &QUdpSocket::readyRead, this, &AirPlayAudioStream::onDataReady);
    connect(m_control.get(), &QUdpSocket::readyRead, this, &AirPlayAudioStream::onControlReady);
#ifdef IVI_FFMPEG_DYN
    if (m_aac)
        m_aacDecoder = new AacDecoder;
#endif
    emit log(QStringLiteral("audio listen data=%1 control=%2 rate=%3 ch=%4 aac=%5")
                 .arg(m_dataPort)
                 .arg(m_controlPort)
                 .arg(m_sampleRate)
                 .arg(m_channels)
                 .arg(m_aac));
    return true;
}

void AirPlayAudioStream::stop()
{
    if (m_data) {
        m_data->disconnect(this);
        m_data->close();
        m_data.reset();
    }
    if (m_control) {
        m_control->disconnect(this);
        m_control->close();
        m_control.reset();
    }
#ifdef IVI_FFMPEG_DYN
    if (m_aacDecoder) {
        static_cast<AacDecoder *>(m_aacDecoder)->close();
        delete static_cast<AacDecoder *>(m_aacDecoder);
        m_aacDecoder = nullptr;
    }
#endif
    m_dataPort = 0;
    m_controlPort = 0;
}

void AirPlayAudioStream::onDataReady()
{
    while (m_data && m_data->hasPendingDatagrams())
        handleDatagram(m_data->receiveDatagram().data());
}

void AirPlayAudioStream::onControlReady()
{
    while (m_control && m_control->hasPendingDatagrams())
        m_control->receiveDatagram();
}

void AirPlayAudioStream::handleDatagram(const QByteArray &wire)
{
    if (wire.size() < kRtpHeader + kTag + kNonce)
        return;
    const QByteArray aad = wire.mid(4, 8);
    const QByteArray sealed = wire.mid(kRtpHeader, wire.size() - kRtpHeader - kNonce);
    QByteArray nonce(12, 0);
    nonce.replace(4, 8, wire.right(kNonce));
    const QByteArray payload = AirPlayCrypto::chachaOpen(m_key, nonce, sealed, aad);
    if (payload.isNull()) {
        ++m_fail;
        if (m_fail <= 3)
            emit log(QStringLiteral("audio decrypt fail #%1").arg(m_fail));
        return;
    }
    ++m_ok;
    if (m_ok <= 3 || (m_ok % 200) == 0)
        emit log(QStringLiteral("audio pkt #%1 bytes=%2").arg(m_ok).arg(payload.size()));

    if (m_aac) {
#ifdef IVI_FFMPEG_DYN
        auto *dec = static_cast<AacDecoder *>(m_aacDecoder);
        if (!dec)
            return;
        const QByteArray pcm = dec->decode(payload);
        if (!pcm.isEmpty())
            emit pcmReady(pcm);
#endif
        return;
    }
    emit pcmReady(payload);
}

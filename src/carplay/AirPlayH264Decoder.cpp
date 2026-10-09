#include "AirPlayH264Decoder.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <cstring>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

QByteArray startCode()
{
    return QByteArray("\x00\x00\x00\x01", 4);
}

QByteArray extractAvcCPayload(const QByteArray &body)
{
    for (int i = 4; i + 4 <= body.size(); ++i) {
        if (std::memcmp(body.constData() + i, "avcC", 4) == 0)
            return body.mid(i + 4);
    }
    return body;
}

bool parseAvcC(const QByteArray &avcC, QByteArray *sps, QByteArray *pps)
{
    if (avcC.size() < 7)
        return false;
    int offset = 5;
    const int spsCount = quint8(avcC[offset]) & 0x1f;
    ++offset;
    for (int i = 0; i < spsCount; ++i) {
        if (offset + 2 > avcC.size())
            return false;
        const int len = (quint8(avcC[offset]) << 8) | quint8(avcC[offset + 1]);
        offset += 2;
        if (offset + len > avcC.size())
            return false;
        if (i == 0)
            *sps = avcC.mid(offset, len);
        offset += len;
    }
    if (offset >= avcC.size())
        return !sps->isEmpty();
    const int ppsCount = quint8(avcC[offset++]);
    for (int i = 0; i < ppsCount; ++i) {
        if (offset + 2 > avcC.size())
            return false;
        const int len = (quint8(avcC[offset]) << 8) | quint8(avcC[offset + 1]);
        offset += 2;
        if (offset + len > avcC.size())
            return false;
        if (i == 0)
            *pps = avcC.mid(offset, len);
        offset += len;
    }
    return !sps->isEmpty() && !pps->isEmpty();
}

QImage yuv420ToRgb(const uchar *yPlane, const uchar *uPlane, const uchar *vPlane,
                   int width, int height, int yStride, int uStride, int vStride)
{
    QImage img(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        auto *line = reinterpret_cast<quint32 *>(img.scanLine(y));
        const uchar *yRow = yPlane + y * yStride;
        const uchar *uRow = uPlane + (y / 2) * uStride;
        const uchar *vRow = vPlane + (y / 2) * vStride;
        int x = 0;
        for (; x + 1 < width; x += 2) {
            const int U = int(uRow[x / 2]) - 128;
            const int V = int(vRow[x / 2]) - 128;
            const int cR = (1436 * V) >> 10;
            const int cG = (352 * U + 731 * V) >> 10;
            const int cB = (1815 * U) >> 10;
            for (int dx = 0; dx < 2; ++dx) {
                const int Y = int(yRow[x + dx]);
                int r = Y + cR;
                int g = Y - cG;
                int b = Y + cB;
                if (r < 0) r = 0; else if (r > 255) r = 255;
                if (g < 0) g = 0; else if (g > 255) g = 255;
                if (b < 0) b = 0; else if (b > 255) b = 255;
                line[x + dx] = 0xff000000u | (quint32(r) << 16) | (quint32(g) << 8) | quint32(b);
            }
        }
        for (; x < width; ++x) {
            const int Y = int(yRow[x]);
            const int U = int(uRow[x / 2]) - 128;
            const int V = int(vRow[x / 2]) - 128;
            int r = Y + ((1436 * V) >> 10);
            int g = Y - ((352 * U + 731 * V) >> 10);
            int b = Y + ((1815 * U) >> 10);
            if (r < 0) r = 0; else if (r > 255) r = 255;
            if (g < 0) g = 0; else if (g > 255) g = 255;
            if (b < 0) b = 0; else if (b > 255) b = 255;
            line[x] = 0xff000000u | (quint32(r) << 16) | (quint32(g) << 8) | quint32(b);
        }
    }
    return img;
}

#ifdef Q_OS_WIN
constexpr int kAvCodecIdH264 = 27;
constexpr int kAvPixFmtYuv420P = 0;
constexpr int kAvPixFmtYuvj420P = 12;
constexpr int kAvPixFmtNv12 = 23;

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

using FnAvcodecFindDecoder = const AVCodec *(*)(int);
using FnAvcodecAllocContext3 = AVCodecContext *(*)(const AVCodec *);
using FnAvcodecOpen2 = int (*)(AVCodecContext *, const AVCodec *, void *);
using FnAvcodecFreeContext = void (*)(AVCodecContext **);
using FnAvcodecSendPacket = int (*)(AVCodecContext *, const AVPacket *);
using FnAvcodecReceiveFrame = int (*)(AVCodecContext *, AVFrame *);
using FnAvcodecFlushBuffers = void (*)(AVCodecContext *);
using FnAvPacketAlloc = AVPacket *(*)();
using FnAvPacketFree = void (*)(AVPacket **);
using FnAvPacketUnref = void (*)(AVPacket *);
using FnAvNewPacket = int (*)(AVPacket *, int);
using FnAvFrameAlloc = AVFrame *(*)();
using FnAvFrameFree = void (*)(AVFrame **);
using FnAvFrameUnref = void (*)(AVFrame *);
#endif

}

struct AirPlayH264Decoder::Impl {
#ifdef Q_OS_WIN
    HMODULE avcodec = nullptr;
    HMODULE avutil = nullptr;
    FnAvcodecFindDecoder findDecoder = nullptr;
    FnAvcodecAllocContext3 allocContext = nullptr;
    FnAvcodecOpen2 open2 = nullptr;
    FnAvcodecFreeContext freeContext = nullptr;
    FnAvcodecSendPacket sendPacket = nullptr;
    FnAvcodecReceiveFrame receiveFrame = nullptr;
    FnAvcodecFlushBuffers flushBuffers = nullptr;
    FnAvPacketAlloc packetAlloc = nullptr;
    FnAvPacketFree packetFree = nullptr;
    FnAvPacketUnref packetUnref = nullptr;
    FnAvNewPacket newPacket = nullptr;
    FnAvFrameAlloc frameAlloc = nullptr;
    FnAvFrameFree frameFree = nullptr;
    FnAvFrameUnref frameUnref = nullptr;

    AVCodecContext *ctx = nullptr;
    AVPacket *pkt = nullptr;
    AVFrame *frame = nullptr;
    QByteArray sps;
    QByteArray pps;
    QString lastError;
    bool started = false;

    HMODULE loadDll(const wchar_t *name)
    {
        const QString path = QDir(QCoreApplication::applicationDirPath())
                                 .filePath(QString::fromWCharArray(name));
        if (QFileInfo::exists(path)) {
            if (const HMODULE mod = LoadLibraryW(reinterpret_cast<LPCWSTR>(path.utf16())))
                return mod;
        }
        return LoadLibraryW(name);
    }

    bool loadLibs()
    {
        if (avcodec && avutil)
            return true;
        const QString appDir = QCoreApplication::applicationDirPath();
        SetDllDirectoryW(reinterpret_cast<LPCWSTR>(appDir.utf16()));
        loadDll(L"libwinpthread-1.dll");
        loadDll(L"zlib1.dll");
        loadDll(L"swresample-6.dll");
        avutil = loadDll(L"avutil-60.dll");
        avcodec = loadDll(L"avcodec-62.dll");
        if (!avcodec || !avutil) {
            lastError = QStringLiteral("load ffmpeg dll failed GetLastError=%1").arg(GetLastError());
            return false;
        }
        findDecoder = reinterpret_cast<FnAvcodecFindDecoder>(GetProcAddress(avcodec, "avcodec_find_decoder"));
        allocContext = reinterpret_cast<FnAvcodecAllocContext3>(GetProcAddress(avcodec, "avcodec_alloc_context3"));
        open2 = reinterpret_cast<FnAvcodecOpen2>(GetProcAddress(avcodec, "avcodec_open2"));
        freeContext = reinterpret_cast<FnAvcodecFreeContext>(GetProcAddress(avcodec, "avcodec_free_context"));
        sendPacket = reinterpret_cast<FnAvcodecSendPacket>(GetProcAddress(avcodec, "avcodec_send_packet"));
        receiveFrame = reinterpret_cast<FnAvcodecReceiveFrame>(GetProcAddress(avcodec, "avcodec_receive_frame"));
        flushBuffers = reinterpret_cast<FnAvcodecFlushBuffers>(GetProcAddress(avcodec, "avcodec_flush_buffers"));
        packetAlloc = reinterpret_cast<FnAvPacketAlloc>(GetProcAddress(avcodec, "av_packet_alloc"));
        packetFree = reinterpret_cast<FnAvPacketFree>(GetProcAddress(avcodec, "av_packet_free"));
        packetUnref = reinterpret_cast<FnAvPacketUnref>(GetProcAddress(avcodec, "av_packet_unref"));
        newPacket = reinterpret_cast<FnAvNewPacket>(GetProcAddress(avcodec, "av_new_packet"));
        frameAlloc = reinterpret_cast<FnAvFrameAlloc>(GetProcAddress(avutil, "av_frame_alloc"));
        frameFree = reinterpret_cast<FnAvFrameFree>(GetProcAddress(avutil, "av_frame_free"));
        frameUnref = reinterpret_cast<FnAvFrameUnref>(GetProcAddress(avutil, "av_frame_unref"));
        if (!findDecoder || !allocContext || !open2 || !freeContext || !sendPacket || !receiveFrame
            || !flushBuffers || !packetAlloc || !packetFree || !packetUnref || !newPacket
            || !frameAlloc || !frameFree || !frameUnref) {
            lastError = QStringLiteral("ffmpeg exports missing");
            return false;
        }
        return true;
    }

    void close()
    {
        if (frame && frameFree)
            frameFree(&frame);
        if (pkt && packetFree)
            packetFree(&pkt);
        if (ctx && freeContext)
            freeContext(&ctx);
        frame = nullptr;
        pkt = nullptr;
        ctx = nullptr;
        started = false;
    }
#endif
};

AirPlayH264Decoder::AirPlayH264Decoder()
    : m(new Impl)
{
}

AirPlayH264Decoder::~AirPlayH264Decoder()
{
#ifdef Q_OS_WIN
    m->close();
#endif
    delete m;
}

bool AirPlayH264Decoder::configure(const QByteArray &avcCIn)
{
#ifdef Q_OS_WIN
    m->close();
    if (!m->loadLibs())
        return false;
    const QByteArray avcC = extractAvcCPayload(avcCIn);
    QByteArray sps;
    QByteArray pps;
    if (!parseAvcC(avcC, &sps, &pps)) {
        m->lastError = QStringLiteral("parseAvcC failed");
        return false;
    }
    m->sps = sps;
    m->pps = pps;

    const AVCodec *codec = m->findDecoder(kAvCodecIdH264);
    if (!codec) {
        m->lastError = QStringLiteral("no h264 decoder");
        return false;
    }
    m->ctx = m->allocContext(codec);
    if (!m->ctx) {
        m->lastError = QStringLiteral("alloc context failed");
        return false;
    }
    if (m->open2(m->ctx, codec, nullptr) < 0) {
        m->lastError = QStringLiteral("avcodec_open2 failed");
        return false;
    }
    m->pkt = m->packetAlloc();
    m->frame = m->frameAlloc();
    if (!m->pkt || !m->frame) {
        m->lastError = QStringLiteral("packet/frame alloc failed");
        return false;
    }
    m->started = true;
    m->lastError = QStringLiteral("ffmpeg ok sps=%1 pps=%2").arg(sps.size()).arg(pps.size());
    return true;
#else
    Q_UNUSED(avcCIn);
    return false;
#endif
}

void AirPlayH264Decoder::flush()
{
#ifdef Q_OS_WIN
    if (m->ctx && m->flushBuffers)
        m->flushBuffers(m->ctx);
#else
#endif
}

QImage AirPlayH264Decoder::decode(const QByteArray &annexB)
{
#ifdef Q_OS_WIN
    if (!m->started || annexB.isEmpty() || !m->ctx || !m->pkt || !m->frame)
        return {};

    QByteArray packet = annexB;
    if (packet.size() >= 5 && (quint8(packet[4]) & 0x1f) == 5)
        packet = startCode() + m->sps + startCode() + m->pps + packet;

    m->packetUnref(m->pkt);
    if (m->newPacket(m->pkt, packet.size()) < 0) {
        m->lastError = QStringLiteral("av_new_packet failed");
        return {};
    }
    {
        auto *lite = reinterpret_cast<AVPacketLite *>(m->pkt);
        std::memcpy(lite->data, packet.constData(), size_t(packet.size()));
        lite->pts = 0;
        lite->dts = 0;
    }

    int err = m->sendPacket(m->ctx, m->pkt);
    if (err < 0) {
        m->lastError = QStringLiteral("send_packet %1").arg(err);
        return {};
    }

    for (int i = 0; i < 4; ++i) {
        m->frameUnref(m->frame);
        err = m->receiveFrame(m->ctx, m->frame);
        if (err == -11) { // EAGAIN
            m->lastError = QStringLiteral("need-more ffmpeg");
            return {};
        }
        if (err < 0) {
            m->lastError = QStringLiteral("receive_frame %1").arg(err);
            return {};
        }

        auto *fr = reinterpret_cast<AVFrameLite *>(m->frame);
        if (fr->width <= 0 || fr->height <= 0 || !fr->data[0])
            continue;

        if ((fr->format == kAvPixFmtYuv420P || fr->format == kAvPixFmtYuvj420P)
            && fr->data[1] && fr->data[2]) {
            return yuv420ToRgb(fr->data[0], fr->data[1], fr->data[2], fr->width, fr->height,
                               fr->linesize[0], fr->linesize[1], fr->linesize[2]);
        }
        if (fr->format == kAvPixFmtNv12 && fr->data[0] && fr->data[1]) {
            QImage img(fr->width, fr->height, QImage::Format_RGB32);
            for (int y = 0; y < fr->height; ++y) {
                QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
                const uchar *yRow = fr->data[0] + y * fr->linesize[0];
                const uchar *uvRow = fr->data[1] + (y / 2) * fr->linesize[1];
                for (int x = 0; x < fr->width; ++x) {
                    const int Y = int(yRow[x]);
                    const int U = int(uvRow[(x & ~1)]) - 128;
                    const int V = int(uvRow[(x & ~1) + 1]) - 128;
                    int r = Y + ((1436 * V) >> 10);
                    int g = Y - ((352 * U + 731 * V) >> 10);
                    int b = Y + ((1815 * U) >> 10);
                    line[x] = qRgb(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255));
                }
            }
            return img;
        }
        m->lastError = QStringLiteral("bad pixfmt %1 %2x%3").arg(fr->format).arg(fr->width).arg(fr->height);
    }
    return {};
#else
    Q_UNUSED(annexB);
    return {};
#endif
}

QString AirPlayH264Decoder::lastError() const
{
#ifdef Q_OS_WIN
    return m->lastError;
#else
    return {};
#endif
}

#include "AirPlayH264Decoder.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
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
                if (unsigned(r) > 255)
                    r = r < 0 ? 0 : 255;
                if (unsigned(g) > 255)
                    g = g < 0 ? 0 : 255;
                if (unsigned(b) > 255)
                    b = b < 0 ? 0 : 255;
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
            if (unsigned(r) > 255)
                r = r < 0 ? 0 : 255;
            if (unsigned(g) > 255)
                g = g < 0 ? 0 : 255;
            if (unsigned(b) > 255)
                b = b < 0 ? 0 : 255;
            line[x] = 0xff000000u | (quint32(r) << 16) | (quint32(g) << 8) | quint32(b);
        }
    }
    return img;
}

QImage nv12ToRgb(const uchar *yPlane, const uchar *uvPlane, int width, int height, int yStride,
                 int uvStride)
{
    QImage img(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        auto *line = reinterpret_cast<quint32 *>(img.scanLine(y));
        const uchar *yRow = yPlane + y * yStride;
        const uchar *uvRow = uvPlane + (y / 2) * uvStride;
        int x = 0;
        for (; x + 1 < width; x += 2) {
            const int U = int(uvRow[x]) - 128;
            const int V = int(uvRow[x + 1]) - 128;
            const int cR = (1436 * V) >> 10;
            const int cG = (352 * U + 731 * V) >> 10;
            const int cB = (1815 * U) >> 10;
            for (int dx = 0; dx < 2; ++dx) {
                const int Y = int(yRow[x + dx]);
                int r = Y + cR;
                int g = Y - cG;
                int b = Y + cB;
                if (unsigned(r) > 255)
                    r = r < 0 ? 0 : 255;
                if (unsigned(g) > 255)
                    g = g < 0 ? 0 : 255;
                if (unsigned(b) > 255)
                    b = b < 0 ? 0 : 255;
                line[x + dx] = 0xff000000u | (quint32(r) << 16) | (quint32(g) << 8) | quint32(b);
            }
        }
        for (; x < width; ++x) {
            const int Y = int(yRow[x]);
            const int U = int(uvRow[x & ~1]) - 128;
            const int V = int(uvRow[(x & ~1) + 1]) - 128;
            int r = Y + ((1436 * V) >> 10);
            int g = Y - ((352 * U + 731 * V) >> 10);
            int b = Y + ((1815 * U) >> 10);
            if (unsigned(r) > 255)
                r = r < 0 ? 0 : 255;
            if (unsigned(g) > 255)
                g = g < 0 ? 0 : 255;
            if (unsigned(b) > 255)
                b = b < 0 ? 0 : 255;
            line[x] = 0xff000000u | (quint32(r) << 16) | (quint32(g) << 8) | quint32(b);
        }
    }
    return img;
}

#ifdef IVI_FFMPEG_DYN
constexpr int kAvCodecIdH264 = 27;
constexpr int kAvPixFmtYuv420P = 0;
constexpr int kAvPixFmtYuvj420P = 12;
constexpr int kAvPixFmtNv12 = 23;
constexpr int kAvPixFmtNv21 = 24;

struct AVPacket;
struct AVFrame;
struct AVCodec;
struct AVCodecContext;
struct AVDictionary;

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
using FnAvcodecFindDecoderByName = const AVCodec *(*)(const char *);
using FnAvcodecAllocContext3 = AVCodecContext *(*)(const AVCodec *);
using FnAvcodecOpen2 = int (*)(AVCodecContext *, const AVCodec *, AVDictionary **);
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

#ifdef Q_OS_WIN
using LibHandle = HMODULE;
static void *sym(LibHandle h, const char *name) { return reinterpret_cast<void *>(GetProcAddress(h, name)); }
#else
using LibHandle = void *;
static void *sym(LibHandle h, const char *name) { return dlsym(h, name); }
#endif
#endif

}

struct AirPlayH264Decoder::Impl {
#ifdef IVI_FFMPEG_DYN
    LibHandle avcodec = nullptr;
    LibHandle avutil = nullptr;
    FnAvcodecFindDecoder findDecoder = nullptr;
    FnAvcodecFindDecoderByName findDecoderByName = nullptr;
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
    QString codecName;
    bool started = false;
    bool hw = false;

#ifdef Q_OS_WIN
    LibHandle loadDll(const wchar_t *name)
    {
        const QString path = QDir(QCoreApplication::applicationDirPath())
                                 .filePath(QString::fromWCharArray(name));
        if (QFileInfo::exists(path)) {
            if (const LibHandle mod = LoadLibraryW(reinterpret_cast<LPCWSTR>(path.utf16())))
                return mod;
        }
        return LoadLibraryW(name);
    }
#else
    LibHandle loadSo(const char *name)
    {
        const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromUtf8(name));
        if (QFileInfo::exists(path)) {
            if (LibHandle mod = dlopen(path.toUtf8().constData(), RTLD_NOW | RTLD_GLOBAL))
                return mod;
        }
        return dlopen(name, RTLD_NOW | RTLD_GLOBAL);
    }
#endif

    bool loadLibs()
    {
        if (avcodec && avutil)
            return true;
#ifdef Q_OS_WIN
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
#else
        static const char *kUtil[] = {"libavutil.so", "libavutil.so.59", "libavutil.so.58", "libavutil.so.57",
                                      nullptr};
        static const char *kCodec[] = {"libavcodec.so", "libavcodec.so.61", "libavcodec.so.60", "libavcodec.so.59",
                                       "libavcodec.so.58", nullptr};
        for (int i = 0; kUtil[i] && !avutil; ++i)
            avutil = loadSo(kUtil[i]);
        for (int i = 0; kCodec[i] && !avcodec; ++i)
            avcodec = loadSo(kCodec[i]);
        if (!avcodec || !avutil) {
            lastError = QStringLiteral("load ffmpeg so failed");
            return false;
        }
#endif
        findDecoder = reinterpret_cast<FnAvcodecFindDecoder>(sym(avcodec, "avcodec_find_decoder"));
        findDecoderByName =
            reinterpret_cast<FnAvcodecFindDecoderByName>(sym(avcodec, "avcodec_find_decoder_by_name"));
        allocContext = reinterpret_cast<FnAvcodecAllocContext3>(sym(avcodec, "avcodec_alloc_context3"));
        open2 = reinterpret_cast<FnAvcodecOpen2>(sym(avcodec, "avcodec_open2"));
        freeContext = reinterpret_cast<FnAvcodecFreeContext>(sym(avcodec, "avcodec_free_context"));
        sendPacket = reinterpret_cast<FnAvcodecSendPacket>(sym(avcodec, "avcodec_send_packet"));
        receiveFrame = reinterpret_cast<FnAvcodecReceiveFrame>(sym(avcodec, "avcodec_receive_frame"));
        flushBuffers = reinterpret_cast<FnAvcodecFlushBuffers>(sym(avcodec, "avcodec_flush_buffers"));
        packetAlloc = reinterpret_cast<FnAvPacketAlloc>(sym(avcodec, "av_packet_alloc"));
        packetFree = reinterpret_cast<FnAvPacketFree>(sym(avcodec, "av_packet_free"));
        packetUnref = reinterpret_cast<FnAvPacketUnref>(sym(avcodec, "av_packet_unref"));
        newPacket = reinterpret_cast<FnAvNewPacket>(sym(avcodec, "av_new_packet"));
        frameAlloc = reinterpret_cast<FnAvFrameAlloc>(sym(avutil, "av_frame_alloc"));
        frameFree = reinterpret_cast<FnAvFrameFree>(sym(avutil, "av_frame_free"));
        frameUnref = reinterpret_cast<FnAvFrameUnref>(sym(avutil, "av_frame_unref"));
        if (!findDecoder || !allocContext || !open2 || !freeContext || !sendPacket || !receiveFrame
            || !flushBuffers || !packetAlloc || !packetFree || !packetUnref || !newPacket
            || !frameAlloc || !frameFree || !frameUnref) {
            lastError = QStringLiteral("ffmpeg exports missing");
            return false;
        }
        return true;
    }

    const AVCodec *pickCodec()
    {
        const QByteArray prefer = qgetenv("IVI_H264_DECODER").trimmed();
        if (!prefer.isEmpty() && findDecoderByName) {
            if (const AVCodec *c = findDecoderByName(prefer.constData())) {
                codecName = QString::fromUtf8(prefer);
                hw = true;
                return c;
            }
        }
        static const char *kHw[] = {
#ifdef Q_OS_WIN
            "h264_mf",
            "h264_qsv",
            "h264_d3d11va",
#else
            "h264_v4l2m2m",
            "h264_omx",
            "h264_rkmpp",
            "h264_v4l2",
#endif
            nullptr};
        if (findDecoderByName) {
            for (int i = 0; kHw[i]; ++i) {
                if (const AVCodec *c = findDecoderByName(kHw[i])) {
                    codecName = QString::fromLatin1(kHw[i]);
                    hw = true;
                    return c;
                }
            }
        }
        codecName = QStringLiteral("h264");
        hw = false;
        return findDecoder(kAvCodecIdH264);
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
        hw = false;
    }
#endif
};

AirPlayH264Decoder::AirPlayH264Decoder()
    : m(new Impl)
{
}

AirPlayH264Decoder::~AirPlayH264Decoder()
{
#ifdef IVI_FFMPEG_DYN
    m->close();
#endif
    delete m;
}

bool AirPlayH264Decoder::preload()
{
#ifdef IVI_FFMPEG_DYN
    return m->loadLibs();
#else
    return false;
#endif
}

bool AirPlayH264Decoder::configure(const QByteArray &avcCIn)
{
#ifdef IVI_FFMPEG_DYN
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

    const AVCodec *codec = m->pickCodec();
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
        if (m->hw) {
            m->close();
            if (!m->loadLibs())
                return false;
            m->sps = sps;
            m->pps = pps;
            m->codecName = QStringLiteral("h264");
            m->hw = false;
            codec = m->findDecoder(kAvCodecIdH264);
            if (!codec) {
                m->lastError = QStringLiteral("hw open failed and no soft h264");
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
        } else {
            m->lastError = QStringLiteral("avcodec_open2 failed");
            return false;
        }
    }
    m->pkt = m->packetAlloc();
    m->frame = m->frameAlloc();
    if (!m->pkt || !m->frame) {
        m->lastError = QStringLiteral("packet/frame alloc failed");
        return false;
    }
    m->started = true;
    m->lastError = QStringLiteral("%1 %2 sps=%3 pps=%4")
                       .arg(m->hw ? QStringLiteral("hw") : QStringLiteral("sw"), m->codecName)
                       .arg(sps.size())
                       .arg(pps.size());
    return true;
#else
    Q_UNUSED(avcCIn);
    return false;
#endif
}

void AirPlayH264Decoder::flush()
{
#ifdef IVI_FFMPEG_DYN
    if (m->ctx && m->flushBuffers)
        m->flushBuffers(m->ctx);
#endif
}

QImage AirPlayH264Decoder::decode(const QByteArray &annexB)
{
#ifdef IVI_FFMPEG_DYN
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

    QImage latest;
    for (int i = 0; i < 8; ++i) {
        m->frameUnref(m->frame);
        err = m->receiveFrame(m->ctx, m->frame);
        if (err == -11) {
            if (latest.isNull())
                m->lastError = QStringLiteral("need-more ffmpeg");
            break;
        }
        if (err < 0) {
            m->lastError = QStringLiteral("receive_frame %1").arg(err);
            break;
        }

        auto *fr = reinterpret_cast<AVFrameLite *>(m->frame);
        if (fr->width <= 0 || fr->height <= 0 || !fr->data[0])
            continue;

        if ((fr->format == kAvPixFmtYuv420P || fr->format == kAvPixFmtYuvj420P)
            && fr->data[1] && fr->data[2]) {
            latest = yuv420ToRgb(fr->data[0], fr->data[1], fr->data[2], fr->width, fr->height,
                                 fr->linesize[0], fr->linesize[1], fr->linesize[2]);
            continue;
        }
        if ((fr->format == kAvPixFmtNv12 || fr->format == kAvPixFmtNv21) && fr->data[0] && fr->data[1]) {
            latest = nv12ToRgb(fr->data[0], fr->data[1], fr->width, fr->height, fr->linesize[0],
                               fr->linesize[1]);
            continue;
        }
        m->lastError = QStringLiteral("bad pixfmt %1 %2x%3").arg(fr->format).arg(fr->width).arg(fr->height);
    }
    return latest;
#else
    Q_UNUSED(annexB);
    return {};
#endif
}

QString AirPlayH264Decoder::lastError() const
{
#ifdef IVI_FFMPEG_DYN
    return m->lastError;
#else
    return {};
#endif
}

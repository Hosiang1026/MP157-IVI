#include "CameraService.hpp"

#include "VehicleState.hpp"

#include <QBuffer>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QThread>
#include <QThreadPool>
#include <QVector>
#include <QtMath>
#include <cstring>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dshow.h>
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#endif

#ifdef Q_OS_LINUX
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

#ifdef Q_OS_WIN

struct AVFormatContext;
struct AVCodecContext;
struct AVCodec;
struct AVCodecParameters;
struct AVPacket;
struct AVFrame;
struct AVDictionary;
struct AVInputFormat;
struct SwsContext;

struct AVFrameLite {
    uint8_t *data[8];
    int linesize[8];
    uint8_t **extended_data;
    int width;
    int height;
    int nb_samples;
    int format;
};

struct AVPacketLite {
    void *buf;
    int64_t pts;
    int64_t dts;
    uint8_t *data;
    int size;
    int stream_index;
};

struct AVStreamLite {
    int index;
    int id;
    AVCodecParameters *codecpar;
};

struct AVFormatLite {
    const void *av_class;
    const void *iformat;
    const void *oformat;
    void *priv_data;
    void *pb;
    int ctx_flags;
    unsigned int nb_streams;
    AVStreamLite **streams;
};

constexpr int kAvMediaTypeVideo = 0;
constexpr int kAvPixFmtRgb24 = 2;
constexpr int kAverrorEagain = -11;
constexpr int kAverrorEof = -541478725;
constexpr int kSwsBilinear = 2;

using FnNetworkInit = int (*)();
using FnFindInputFormat = AVInputFormat *(*)(const char *);
using FnOpenInput = int (*)(AVFormatContext **, const char *, AVInputFormat *, AVDictionary **);
using FnFindStreamInfo = int (*)(AVFormatContext *, AVDictionary **);
using FnCloseInput = void (*)(AVFormatContext **);
using FnFindBestStream = int (*)(AVFormatContext *, int, int, int, const AVCodec **, int);
using FnReadFrame = int (*)(AVFormatContext *, AVPacket *);
using FnFindDecoder = const AVCodec *(*)(int);
using FnAllocContext = AVCodecContext *(*)(const AVCodec *);
using FnParamsToContext = int (*)(AVCodecContext *, const AVCodecParameters *);
using FnOpen2 = int (*)(AVCodecContext *, const AVCodec *, AVDictionary **);
using FnFreeContext = void (*)(AVCodecContext **);
using FnSendPacket = int (*)(AVCodecContext *, const AVPacket *);
using FnReceiveFrame = int (*)(AVCodecContext *, AVFrame *);
using FnPacketAlloc = AVPacket *(*)();
using FnPacketFree = void (*)(AVPacket **);
using FnPacketUnref = void (*)(AVPacket *);
using FnFrameAlloc = AVFrame *(*)();
using FnFrameFree = void (*)(AVFrame **);
using FnFrameUnref = void (*)(AVFrame *);
using FnDictSet = int (*)(AVDictionary **, const char *, const char *, int);
using FnDictFree = void (*)(AVDictionary **);
using FnSwsGetContext = SwsContext *(*)(int, int, int, int, int, int, int, void *, void *, void *);
using FnSwsScale = int (*)(SwsContext *, const uint8_t *const[], const int[], int, int, uint8_t *const[],
                           const int[]);
using FnSwsFreeContext = void (*)(SwsContext *);

struct Libs {
    HMODULE avutil = nullptr;
    HMODULE avcodec = nullptr;
    HMODULE avformat = nullptr;
    HMODULE swscale = nullptr;

    FnNetworkInit networkInit = nullptr;
    FnFindInputFormat findInputFormat = nullptr;
    FnOpenInput openInput = nullptr;
    FnFindStreamInfo findStreamInfo = nullptr;
    FnCloseInput closeInput = nullptr;
    FnFindBestStream findBestStream = nullptr;
    FnReadFrame readFrame = nullptr;
    FnFindDecoder findDecoder = nullptr;
    FnAllocContext allocContext = nullptr;
    FnParamsToContext paramsToContext = nullptr;
    FnOpen2 open2 = nullptr;
    FnFreeContext freeContext = nullptr;
    FnSendPacket sendPacket = nullptr;
    FnReceiveFrame receiveFrame = nullptr;
    FnPacketAlloc packetAlloc = nullptr;
    FnPacketFree packetFree = nullptr;
    FnPacketUnref packetUnref = nullptr;
    FnFrameAlloc frameAlloc = nullptr;
    FnFrameFree frameFree = nullptr;
    FnFrameUnref frameUnref = nullptr;
    FnDictSet dictSet = nullptr;
    FnDictFree dictFree = nullptr;
    FnSwsGetContext swsGet = nullptr;
    FnSwsScale swsScale = nullptr;
    FnSwsFreeContext swsFree = nullptr;

    HMODULE loadDll(const wchar_t *name)
    {
        const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromWCharArray(name));
        if (QFileInfo::exists(path)) {
            if (const HMODULE mod = LoadLibraryW(reinterpret_cast<LPCWSTR>(path.utf16())))
                return mod;
        }
        return LoadLibraryW(name);
    }

    bool load()
    {
        if (avformat && avcodec && avutil && swscale)
            return true;
        const QString appDir = QCoreApplication::applicationDirPath();
        SetDllDirectoryW(reinterpret_cast<LPCWSTR>(appDir.utf16()));
        loadDll(L"libwinpthread-1.dll");
        loadDll(L"zlib1.dll");
        avutil = loadDll(L"avutil-60.dll");
        loadDll(L"swresample-6.dll");
        avcodec = loadDll(L"avcodec-62.dll");
        avformat = loadDll(L"avformat-62.dll");
        swscale = loadDll(L"swscale-9.dll");
        if (!avutil || !avcodec || !avformat || !swscale)
            return false;

        networkInit = reinterpret_cast<FnNetworkInit>(GetProcAddress(avformat, "avformat_network_init"));
        findInputFormat = reinterpret_cast<FnFindInputFormat>(GetProcAddress(avformat, "av_find_input_format"));
        openInput = reinterpret_cast<FnOpenInput>(GetProcAddress(avformat, "avformat_open_input"));
        findStreamInfo = reinterpret_cast<FnFindStreamInfo>(GetProcAddress(avformat, "avformat_find_stream_info"));
        closeInput = reinterpret_cast<FnCloseInput>(GetProcAddress(avformat, "avformat_close_input"));
        findBestStream = reinterpret_cast<FnFindBestStream>(GetProcAddress(avformat, "av_find_best_stream"));
        readFrame = reinterpret_cast<FnReadFrame>(GetProcAddress(avformat, "av_read_frame"));
        findDecoder = reinterpret_cast<FnFindDecoder>(GetProcAddress(avcodec, "avcodec_find_decoder"));
        allocContext = reinterpret_cast<FnAllocContext>(GetProcAddress(avcodec, "avcodec_alloc_context3"));
        paramsToContext = reinterpret_cast<FnParamsToContext>(GetProcAddress(avcodec, "avcodec_parameters_to_context"));
        open2 = reinterpret_cast<FnOpen2>(GetProcAddress(avcodec, "avcodec_open2"));
        freeContext = reinterpret_cast<FnFreeContext>(GetProcAddress(avcodec, "avcodec_free_context"));
        sendPacket = reinterpret_cast<FnSendPacket>(GetProcAddress(avcodec, "avcodec_send_packet"));
        receiveFrame = reinterpret_cast<FnReceiveFrame>(GetProcAddress(avcodec, "avcodec_receive_frame"));
        packetAlloc = reinterpret_cast<FnPacketAlloc>(GetProcAddress(avcodec, "av_packet_alloc"));
        packetFree = reinterpret_cast<FnPacketFree>(GetProcAddress(avcodec, "av_packet_free"));
        packetUnref = reinterpret_cast<FnPacketUnref>(GetProcAddress(avcodec, "av_packet_unref"));
        frameAlloc = reinterpret_cast<FnFrameAlloc>(GetProcAddress(avutil, "av_frame_alloc"));
        frameFree = reinterpret_cast<FnFrameFree>(GetProcAddress(avutil, "av_frame_free"));
        frameUnref = reinterpret_cast<FnFrameUnref>(GetProcAddress(avutil, "av_frame_unref"));
        dictSet = reinterpret_cast<FnDictSet>(GetProcAddress(avutil, "av_dict_set"));
        dictFree = reinterpret_cast<FnDictFree>(GetProcAddress(avutil, "av_dict_free"));
        swsGet = reinterpret_cast<FnSwsGetContext>(GetProcAddress(swscale, "sws_getContext"));
        swsScale = reinterpret_cast<FnSwsScale>(GetProcAddress(swscale, "sws_scale"));
        swsFree = reinterpret_cast<FnSwsFreeContext>(GetProcAddress(swscale, "sws_freeContext"));

        return networkInit && findInputFormat && openInput && findStreamInfo && closeInput && findBestStream
            && readFrame && findDecoder && allocContext && paramsToContext && open2 && freeContext && sendPacket
            && receiveFrame && packetAlloc && packetFree && packetUnref && frameAlloc && frameFree && frameUnref
            && dictSet && dictFree && swsGet && swsScale && swsFree;
    }
};

AVCodecParameters *paramsOf(AVFormatContext *fmt, int index)
{
    auto *lite = reinterpret_cast<AVFormatLite *>(fmt);
    if (!lite || !lite->streams || index < 0 || unsigned(index) >= lite->nb_streams)
        return nullptr;
    return lite->streams[index]->codecpar;
}

int codecIdOf(AVCodecParameters *par)
{
    return reinterpret_cast<const int *>(par)[1];
}

QStringList listDshowDevices()
{
    QStringList out;
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
        CoInitialize(nullptr);
    ICreateDevEnum *devEnum = nullptr;
    if (FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum,
                                reinterpret_cast<void **>(&devEnum)))
        || !devEnum)
        return out;
    IEnumMoniker *enumMon = nullptr;
    const HRESULT hr = devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumMon, 0);
    if (hr != S_OK || !enumMon) {
        devEnum->Release();
        return out;
    }
    IMoniker *mon = nullptr;
    while (enumMon->Next(1, &mon, nullptr) == S_OK && mon) {
        IPropertyBag *prop = nullptr;
        if (SUCCEEDED(mon->BindToStorage(nullptr, nullptr, IID_IPropertyBag, reinterpret_cast<void **>(&prop)))
            && prop) {
            VARIANT name;
            VariantInit(&name);
            if (SUCCEEDED(prop->Read(L"FriendlyName", &name, nullptr)) && name.vt == VT_BSTR && name.bstrVal)
                out.append(QString::fromWCharArray(name.bstrVal));
            VariantClear(&name);
            prop->Release();
        }
        mon->Release();
    }
    enumMon->Release();
    devEnum->Release();
    return out;
}

#endif

#ifdef Q_OS_LINUX

QStringList listV4lDevices()
{
    QStringList out;
    const QDir dir(QStringLiteral("/dev"));
    const auto entries = dir.entryList({QStringLiteral("video*")}, QDir::System, QDir::Name);
    for (const QString &e : entries)
        out.append(dir.filePath(e));
    return out;
}

QImage yuyvToRgb(const uchar *yuyv, int w, int h)
{
    QImage img(w, h, QImage::Format_RGB888);
    for (int y = 0; y < h; ++y) {
        const uchar *src = yuyv + y * w * 2;
        uchar *dst = img.scanLine(y);
        for (int x = 0; x < w; x += 2) {
            const int y0 = src[0];
            const int u = src[1] - 128;
            const int y1 = src[2];
            const int v = src[3] - 128;
            src += 4;
            auto put = [&](int Y, uchar *p) {
                const int c = Y - 16;
                const int r = qBound(0, (298 * c + 409 * v + 128) >> 8, 255);
                const int g = qBound(0, (298 * c - 100 * u - 208 * v + 128) >> 8, 255);
                const int b = qBound(0, (298 * c + 516 * u + 128) >> 8, 255);
                p[0] = uchar(r);
                p[1] = uchar(g);
                p[2] = uchar(b);
            };
            put(y0, dst);
            put(y1, dst + 3);
            dst += 6;
        }
    }
    return img;
}

#endif

void writeFourcc(QIODevice *dev, const char *fcc)
{
    dev->write(fcc, 4);
}

void writeU32(QIODevice *dev, quint32 v)
{
    char b[4] = {char(v & 0xff), char((v >> 8) & 0xff), char((v >> 16) & 0xff), char((v >> 24) & 0xff)};
    dev->write(b, 4);
}

void writeU16(QIODevice *dev, quint16 v)
{
    char b[2] = {char(v & 0xff), char((v >> 8) & 0xff)};
    dev->write(b, 2);
}

} // namespace

CameraService::CameraService(VehicleState *vehicle, QObject *parent)
    : QObject(parent)
    , m_vehicle(vehicle)
{
    m_recordTick.setInterval(1000);
    connect(&m_recordTick, &QTimer::timeout, this, [this] {
        if (!m_recording.load())
            return;
        ++m_recordSeconds;
        emit stateChanged();
    });
    QSettings s;
    m_deviceName = s.value(QStringLiteral("camera/device")).toString();
    refreshDevices();
    if (m_vehicle)
        connect(m_vehicle, &VehicleState::changed, this, &CameraService::onVehicleChanged);
    onVehicleChanged();
}

CameraService::~CameraService()
{
    stopRecording();
    m_holders.clear();
    stopCapture();
    while (m_workerBusy.load())
        QThread::msleep(10);
}

bool CameraService::hasFrame() const
{
    QMutexLocker lock(&m_frameMutex);
    return !m_frame.isNull();
}

QImage CameraService::currentFrame() const
{
    QMutexLocker lock(&m_frameMutex);
    return m_frame;
}

void CameraService::refreshDevices()
{
#ifdef Q_OS_WIN
    m_devices = listDshowDevices();
#elif defined(Q_OS_LINUX)
    m_devices = listV4lDevices();
#else
    m_devices.clear();
#endif
    if (m_deviceName.isEmpty() && !m_devices.isEmpty())
        m_deviceName = m_devices.first();
    emit devicesChanged();
    emit stateChanged();
}

void CameraService::setDevice(const QString &name)
{
    if (name == m_deviceName)
        return;
    m_deviceName = name;
    QSettings s;
    s.setValue(QStringLiteral("camera/device"), m_deviceName);
    emit stateChanged();
    if (m_active.load()) {
        stopCapture();
        ensureRunning();
    }
}

void CameraService::acquire(const QString &holder)
{
    if (holder.isEmpty())
        return;
    if (!m_holders.contains(holder))
        m_holders.append(holder);
    ensureRunning();
    emit stateChanged();
}

void CameraService::release(const QString &holder)
{
    m_holders.removeAll(holder);
    if (m_holders.isEmpty()) {
        if (m_recording.load())
            stopRecording();
        stopCapture();
    }
    emit stateChanged();
}

void CameraService::onVehicleChanged()
{
    if (!m_vehicle)
        return;
    if (m_vehicle->gear() == QStringLiteral("R"))
        acquire(QStringLiteral("reverse"));
    else
        release(QStringLiteral("reverse"));
}

void CameraService::ensureRunning()
{
    if (m_holders.isEmpty() || m_workerBusy.load())
        return;
    m_stop.store(false);
    m_active.store(true);
    m_demoMode = false;
    setStatus(QStringLiteral("启动中…"));
    const QString device = m_deviceName;
    m_workerBusy.store(true);
    QThreadPool::globalInstance()->start([this, device] {
        const bool ok = openRealCapture(device);
        if (!ok && !m_stop.load())
            demoLoop();
        m_active.store(false);
        m_workerBusy.store(false);
        QMetaObject::invokeMethod(this, [this] {
            if (m_holders.isEmpty())
                setStatus(QStringLiteral("已停止"));
            else
                ensureRunning();
            emit stateChanged();
        }, Qt::QueuedConnection);
    });
    emit stateChanged();
}

void CameraService::stopCapture()
{
    m_stop.store(true);
}

void CameraService::setStatus(const QString &text)
{
    if (m_status == text)
        return;
    m_status = text;
    emit stateChanged();
}

void CameraService::pushFrame(const QImage &img)
{
    {
        QMutexLocker lock(&m_frameMutex);
        m_frame = img;
    }
    if (m_recording.load()) {
        QMutexLocker lock(&m_recordMutex);
        writeRecordFrame(img);
    }
    emit frameChanged();
}

bool CameraService::openRealCapture(const QString &device)
{
#ifdef Q_OS_WIN
    if (device.isEmpty())
        return false;
    Libs lib;
    if (!lib.load()) {
        QMetaObject::invokeMethod(this, [this] { setStatus(QStringLiteral("缺少 FFmpeg DLL")); }, Qt::QueuedConnection);
        return false;
    }
    lib.networkInit();
    AVInputFormat *iformat = lib.findInputFormat("dshow");
    if (!iformat) {
        QMetaObject::invokeMethod(this, [this] { setStatus(QStringLiteral("无 dshow 支持")); }, Qt::QueuedConnection);
        return false;
    }
    const QByteArray url = QStringLiteral("video=%1").arg(device).toUtf8();
    AVFormatContext *fmt = nullptr;
    AVDictionary *opts = nullptr;
    lib.dictSet(&opts, "rtbufsize", "100000000", 0);
    lib.dictSet(&opts, "framerate", "25", 0);
    if (lib.openInput(&fmt, url.constData(), iformat, &opts) < 0 || !fmt) {
        lib.dictFree(&opts);
        QMetaObject::invokeMethod(this, [this] { setStatus(QStringLiteral("打开摄像头失败")); }, Qt::QueuedConnection);
        return false;
    }
    lib.dictFree(&opts);
    if (lib.findStreamInfo(fmt, nullptr) < 0) {
        lib.closeInput(&fmt);
        return false;
    }
    const AVCodec *vCodec = nullptr;
    const int vIdx = lib.findBestStream(fmt, kAvMediaTypeVideo, -1, -1, &vCodec, 0);
    if (vIdx < 0) {
        lib.closeInput(&fmt);
        return false;
    }
    if (!vCodec)
        vCodec = lib.findDecoder(codecIdOf(paramsOf(fmt, vIdx)));
    AVCodecContext *vCtx = lib.allocContext(vCodec);
    AVCodecParameters *par = paramsOf(fmt, vIdx);
    if (!vCodec || !vCtx || !par || lib.paramsToContext(vCtx, par) < 0 || lib.open2(vCtx, vCodec, nullptr) < 0) {
        if (vCtx)
            lib.freeContext(&vCtx);
        lib.closeInput(&fmt);
        return false;
    }

    AVPacket *pkt = lib.packetAlloc();
    AVFrame *frame = lib.frameAlloc();
    SwsContext *sws = nullptr;
    QByteArray rgb;
    int rgbW = 0;
    int rgbH = 0;
    m_demoMode = false;
    QMetaObject::invokeMethod(this, [this, device] {
        setStatus(QStringLiteral("摄像头 · %1").arg(device));
        emit stateChanged();
    }, Qt::QueuedConnection);

    auto ensureSws = [&](int w, int h, int pix) {
        if (sws && rgbW == w && rgbH == h)
            return true;
        if (sws) {
            lib.swsFree(sws);
            sws = nullptr;
        }
        sws = lib.swsGet(w, h, pix, w, h, kAvPixFmtRgb24, kSwsBilinear, nullptr, nullptr, nullptr);
        if (!sws)
            return false;
        rgbW = w;
        rgbH = h;
        rgb.resize(w * h * 3);
        return true;
    };

    while (!m_stop.load()) {
        lib.packetUnref(pkt);
        const int rr = lib.readFrame(fmt, pkt);
        if (rr == kAverrorEof || rr < 0)
            break;
        if (reinterpret_cast<AVPacketLite *>(pkt)->stream_index != vIdx)
            continue;
        if (lib.sendPacket(vCtx, pkt) < 0)
            continue;
        while (!m_stop.load()) {
            lib.frameUnref(frame);
            const int got = lib.receiveFrame(vCtx, frame);
            if (got == kAverrorEagain || got == kAverrorEof)
                break;
            if (got < 0)
                break;
            auto *fr = reinterpret_cast<AVFrameLite *>(frame);
            if (fr->width <= 0 || fr->height <= 0 || !fr->data[0])
                continue;
            if (!ensureSws(fr->width, fr->height, fr->format))
                continue;
            uint8_t *dst[4] = {reinterpret_cast<uint8_t *>(rgb.data()), nullptr, nullptr, nullptr};
            int dstStride[4] = {rgbW * 3, 0, 0, 0};
            lib.swsScale(sws, fr->data, fr->linesize, 0, fr->height, dst, dstStride);
            QImage img(rgbW, rgbH, QImage::Format_RGB888);
            for (int y = 0; y < rgbH; ++y)
                std::memcpy(img.scanLine(y), rgb.constData() + y * rgbW * 3, size_t(rgbW * 3));
            pushFrame(img.copy());
        }
    }

    if (sws)
        lib.swsFree(sws);
    if (frame)
        lib.frameFree(&frame);
    if (pkt)
        lib.packetFree(&pkt);
    lib.freeContext(&vCtx);
    lib.closeInput(&fmt);
    return true;
#elif defined(Q_OS_LINUX)
    QString path = device;
    if (path.isEmpty()) {
        const auto list = listV4lDevices();
        if (list.isEmpty())
            return false;
        path = list.first();
    }
    const int fd = ::open(path.toUtf8().constData(), O_RDWR | O_NONBLOCK, 0);
    if (fd < 0)
        return false;

    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
            ::close(fd);
            return false;
        }
    }
    const bool mjpeg = fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG;
    const int width = int(fmt.fmt.pix.width);
    const int height = int(fmt.fmt.pix.height);

    v4l2_requestbuffers req{};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) {
        ::close(fd);
        return false;
    }

    struct Buf {
        void *start = nullptr;
        size_t length = 0;
    };
    QVector<Buf> bufs(int(req.count));
    for (unsigned i = 0; i < req.count; ++i) {
        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            ::close(fd);
            return false;
        }
        bufs[int(i)].length = buf.length;
        bufs[int(i)].start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (bufs[int(i)].start == MAP_FAILED) {
            ::close(fd);
            return false;
        }
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            ::close(fd);
            return false;
        }
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        ::close(fd);
        return false;
    }

    m_demoMode = false;
    QMetaObject::invokeMethod(this, [this, path] {
        setStatus(QStringLiteral("摄像头 · %1").arg(path));
        emit stateChanged();
    }, Qt::QueuedConnection);

    while (!m_stop.load()) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        timeval tv{0, 200000};
        const int r = select(fd + 1, &fds, nullptr, nullptr, &tv);
        if (r <= 0)
            continue;
        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0)
            continue;
        const Buf &b = bufs[int(buf.index)];
        QImage img;
        if (mjpeg) {
            img = QImage::fromData(reinterpret_cast<const uchar *>(b.start), int(buf.bytesused), "JPEG");
        } else {
            img = yuyvToRgb(reinterpret_cast<const uchar *>(b.start), width, height);
        }
        if (!img.isNull())
            pushFrame(img.convertToFormat(QImage::Format_RGB888));
        ioctl(fd, VIDIOC_QBUF, &buf);
    }

    ioctl(fd, VIDIOC_STREAMOFF, &type);
    for (const Buf &b : bufs) {
        if (b.start && b.start != MAP_FAILED)
            munmap(b.start, b.length);
    }
    ::close(fd);
    return true;
#else
    Q_UNUSED(device);
    return false;
#endif
}

void CameraService::demoLoop()
{
    m_demoMode = true;
    QMetaObject::invokeMethod(this, [this] {
        setStatus(QStringLiteral("演示画面（未检测到摄像头）"));
        emit stateChanged();
    }, Qt::QueuedConnection);

    int t = 0;
    while (!m_stop.load()) {
        QImage img(640, 360, QImage::Format_RGB32);
        img.fill(QColor(18, 22, 28));
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        for (int x = 0; x < 640; x += 40) {
            p.setPen(QColor(40, 48, 58));
            p.drawLine(x, 0, x, 360);
        }
        for (int y = 0; y < 360; y += 40) {
            p.setPen(QColor(40, 48, 58));
            p.drawLine(0, y, 640, y);
        }
        const qreal phase = t * 0.08;
        QPainterPath guide;
        guide.moveTo(80, 340);
        guide.lineTo(220 + 20 * qSin(phase), 120);
        guide.lineTo(420 - 20 * qSin(phase), 120);
        guide.lineTo(560, 340);
        p.setPen(QPen(QColor(255, 204, 0), 4));
        p.drawPath(guide);
        p.setPen(QPen(QColor(255, 59, 48), 3, Qt::DashLine));
        p.drawLine(320, 100, 320, 340);
        p.setPen(Qt::white);
        p.setFont(QFont(QStringLiteral("Sans Serif"), 22, QFont::Bold));
        p.drawText(QRect(0, 16, 640, 40), Qt::AlignHCenter, QStringLiteral("倒车影像 / 行车记录仪"));
        p.setFont(QFont(QStringLiteral("Sans Serif"), 12));
        p.setPen(QColor(160, 168, 180));
        p.drawText(QRect(0, 56, 640, 24), Qt::AlignHCenter, QStringLiteral("软件预览 · 接入摄像头后显示实况"));
        p.end();
        pushFrame(img.convertToFormat(QImage::Format_RGB888));
        ++t;
        QThread::msleep(40);
    }
}

QString CameraService::recordingsDir() const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("recordings"));
}

void CameraService::startRecording()
{
    if (m_recording.load())
        return;
    if (m_holders.isEmpty())
        acquire(QStringLiteral("record"));
    QDir().mkpath(recordingsDir());
    const QString name = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")) + QStringLiteral(".avi");
    m_recordPath = QDir(recordingsDir()).filePath(name);
    {
        QMutexLocker lock(&m_recordMutex);
        delete m_aviFile;
        m_aviFile = new QFile(m_recordPath);
        if (!m_aviFile->open(QIODevice::WriteOnly)) {
            delete m_aviFile;
            m_aviFile = nullptr;
            setStatus(QStringLiteral("无法创建录像文件"));
            return;
        }
        m_aviIndex.clear();
        m_aviFrames = 0;
        m_aviW = 0;
        m_aviH = 0;

    writeFourcc(m_aviFile, "RIFF");
    writeU32(m_aviFile, 0);
    writeFourcc(m_aviFile, "AVI ");
    writeFourcc(m_aviFile, "LIST");
    writeU32(m_aviFile, 192);
    writeFourcc(m_aviFile, "hdrl");
    writeFourcc(m_aviFile, "avih");
    writeU32(m_aviFile, 56);
    writeU32(m_aviFile, 40000);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0x10);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 1);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 640);
    writeU32(m_aviFile, 360);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0);
    writeFourcc(m_aviFile, "LIST");
    writeU32(m_aviFile, 116);
    writeFourcc(m_aviFile, "strl");
    writeFourcc(m_aviFile, "strh");
    writeU32(m_aviFile, 56);
    writeFourcc(m_aviFile, "vids");
    writeFourcc(m_aviFile, "MJPG");
    writeU32(m_aviFile, 0);
    writeU16(m_aviFile, 0);
    writeU16(m_aviFile, 0);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 1);
    writeU32(m_aviFile, 25);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0xffffffff);
    writeU32(m_aviFile, 0);
    writeU16(m_aviFile, 0);
    writeU16(m_aviFile, 0);
    writeU16(m_aviFile, 0);
    writeU16(m_aviFile, 0);
    writeFourcc(m_aviFile, "strf");
    writeU32(m_aviFile, 40);
    writeU32(m_aviFile, 40);
    writeU32(m_aviFile, 640);
    writeU32(m_aviFile, 360);
    writeU16(m_aviFile, 1);
    writeU16(m_aviFile, 24);
    writeFourcc(m_aviFile, "MJPG");
    writeU32(m_aviFile, 640 * 360 * 3);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0);
    writeFourcc(m_aviFile, "LIST");
    m_aviMoviSizePos = m_aviFile->pos();
    writeU32(m_aviFile, 0);
    writeFourcc(m_aviFile, "movi");
    }

    m_recordSeconds = 0;
    m_recording.store(true);
    m_recordTick.start();
    setStatus(QStringLiteral("录像中…"));
    emit stateChanged();
}

void CameraService::writeRecordFrame(const QImage &img)
{
    if (!m_aviFile || !m_aviFile->isOpen() || img.isNull())
        return;
    QImage frame = img;
    if (m_aviW == 0) {
        m_aviW = frame.width() > 0 ? frame.width() : 640;
        m_aviH = frame.height() > 0 ? frame.height() : 360;
    } else if (frame.width() != m_aviW || frame.height() != m_aviH) {
        frame = frame.scaled(m_aviW, m_aviH, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    buffer.open(QIODevice::WriteOnly);
    frame.save(&buffer, "JPEG", 80);
    if (jpeg.isEmpty())
        return;
    if (jpeg.size() & 1)
        jpeg.append(char(0));

    const qint64 chunkPos = m_aviFile->pos();
    writeFourcc(m_aviFile, "00dc");
    writeU32(m_aviFile, quint32(jpeg.size()));
    m_aviFile->write(jpeg);

    const quint32 off = quint32(chunkPos - (m_aviMoviSizePos + 4));
    char idx[16] = {'0', '0', 'd', 'c', 0x10, 0, 0, 0};
    idx[8] = char(off & 0xff);
    idx[9] = char((off >> 8) & 0xff);
    idx[10] = char((off >> 16) & 0xff);
    idx[11] = char((off >> 24) & 0xff);
    const quint32 sz = quint32(jpeg.size());
    idx[12] = char(sz & 0xff);
    idx[13] = char((sz >> 8) & 0xff);
    idx[14] = char((sz >> 16) & 0xff);
    idx[15] = char((sz >> 24) & 0xff);
    m_aviIndex.append(idx, 16);
    ++m_aviFrames;
}

void CameraService::finalizeAvi()
{
    if (!m_aviFile || !m_aviFile->isOpen())
        return;
    const qint64 moviEnd = m_aviFile->pos();
    m_aviFile->seek(m_aviMoviSizePos);
    writeU32(m_aviFile, quint32(moviEnd - m_aviMoviSizePos - 4));
    m_aviFile->seek(moviEnd);
    writeFourcc(m_aviFile, "idx1");
    writeU32(m_aviFile, quint32(m_aviIndex.size()));
    m_aviFile->write(m_aviIndex);
    const qint64 fileSize = m_aviFile->pos();
    m_aviFile->seek(4);
    writeU32(m_aviFile, quint32(fileSize - 8));

    const int w = m_aviW > 0 ? m_aviW : 640;
    const int h = m_aviH > 0 ? m_aviH : 360;
    m_aviFile->seek(32);
    writeU32(m_aviFile, 40000);
    writeU32(m_aviFile, quint32(w * h * 3));
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0x10);
    writeU32(m_aviFile, quint32(m_aviFrames));
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 1);
    writeU32(m_aviFile, quint32(w * h * 3));
    writeU32(m_aviFile, quint32(w));
    writeU32(m_aviFile, quint32(h));

    m_aviFile->seek(108);
    writeFourcc(m_aviFile, "vids");
    writeFourcc(m_aviFile, "MJPG");
    writeU32(m_aviFile, 0);
    writeU16(m_aviFile, 0);
    writeU16(m_aviFile, 0);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 1);
    writeU32(m_aviFile, 25);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, quint32(m_aviFrames));
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0xffffffff);
    writeU32(m_aviFile, 0);
    writeU16(m_aviFile, 0);
    writeU16(m_aviFile, 0);
    writeU16(m_aviFile, quint16(w));
    writeU16(m_aviFile, quint16(h));

    m_aviFile->seek(172);
    writeU32(m_aviFile, 40);
    writeU32(m_aviFile, quint32(w));
    writeU32(m_aviFile, quint32(h));
    writeU16(m_aviFile, 1);
    writeU16(m_aviFile, 24);
    writeFourcc(m_aviFile, "MJPG");
    writeU32(m_aviFile, quint32(w * h * 3));
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0);
    writeU32(m_aviFile, 0);
    m_aviFile->close();
}

void CameraService::stopRecording()
{
    if (!m_recording.exchange(false)) {
        if (m_holders.contains(QStringLiteral("record")))
            release(QStringLiteral("record"));
        return;
    }
    m_recordTick.stop();
    {
        QMutexLocker lock(&m_recordMutex);
        finalizeAvi();
        delete m_aviFile;
        m_aviFile = nullptr;
    }
    setStatus(QStringLiteral("已保存 %1").arg(QFileInfo(m_recordPath).fileName()));
    if (m_holders.contains(QStringLiteral("record")))
        release(QStringLiteral("record"));
    emit stateChanged();
}

QVariantList CameraService::listRecordings() const
{
    QVariantList out;
    QDir dir(recordingsDir());
    const auto files = dir.entryInfoList({QStringLiteral("*.avi")}, QDir::Files, QDir::Time);
    for (const QFileInfo &fi : files) {
        QVariantMap m;
        m.insert(QStringLiteral("name"), fi.fileName());
        m.insert(QStringLiteral("path"), fi.absoluteFilePath());
        m.insert(QStringLiteral("size"), fi.size());
        out.append(m);
    }
    return out;
}

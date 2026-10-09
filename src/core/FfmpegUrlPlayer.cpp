#include "FfmpegUrlPlayer.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QThreadPool>
#include <cstring>

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
#define IVI_FFMPEG_DYN 1
#endif

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#elif defined(Q_OS_LINUX)
#include <dlfcn.h>
#if defined(IVI_HAVE_ALSA)
#include <alsa/asoundlib.h>
#endif
#endif

namespace {

#ifdef IVI_FFMPEG_DYN

#ifdef Q_OS_WIN
using LibHandle = HMODULE;
static void *ffSym(LibHandle h, const char *n) { return reinterpret_cast<void *>(GetProcAddress(h, n)); }
#else
using LibHandle = void *;
static void *ffSym(LibHandle h, const char *n) { return dlsym(h, n); }
#endif

struct AVFormatContext;
struct AVCodecContext;
struct AVCodec;
struct AVCodecParameters;
struct AVPacket;
struct AVFrame;
struct AVDictionary;
struct SwsContext;
struct SwrContext;

struct AVChannelLayout {
    int order;
    int nb_channels;
    uint64_t u_mask;
    void *opaque;
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
constexpr int kAvMediaTypeAudio = 1;
constexpr int kAvSampleFmtS16 = 1;
constexpr int kAvPixFmtRgb24 = 2;
constexpr int kAverrorEagain = -11;
constexpr int kAverrorEof = -541478725;
constexpr int kSwsBilinear = 2;

using FnNetworkInit = int (*)();
using FnOpenInput = int (*)(AVFormatContext **, const char *, void *, AVDictionary **);
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
using FnOptGetInt = int (*)(void *, const char *, int, int64_t *);
using FnSwsGetContext = SwsContext *(*)(int, int, int, int, int, int, int, void *, void *, void *);
using FnSwsScale = int (*)(SwsContext *, const uint8_t *const[], const int[], int, int, uint8_t *const[],
                           const int[]);
using FnSwsFreeContext = void (*)(SwsContext *);
using FnSwrAllocSetOpts2 = int (*)(SwrContext **, const AVChannelLayout *, int, int, const AVChannelLayout *,
                                   int, int, int, void *);
using FnSwrAllocSetOpts = SwrContext *(*)(SwrContext *, int64_t, int, int, int64_t, int, int, int, void *);
using FnSwrInit = int (*)(SwrContext *);
using FnSwrConvert = int (*)(SwrContext *, uint8_t **, int, const uint8_t **const, int);
using FnSwrFree = void (*)(SwrContext **);
using FnChLayoutDefault = void (*)(AVChannelLayout *, int);
using FnChLayoutUninit = void (*)(AVChannelLayout *);

struct Libs {
    LibHandle avutil = nullptr;
    LibHandle avcodec = nullptr;
    LibHandle avformat = nullptr;
    LibHandle swscale = nullptr;
    LibHandle swresample = nullptr;

    FnNetworkInit networkInit = nullptr;
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
    FnOptGetInt optGetInt = nullptr;
    FnSwsGetContext swsGet = nullptr;
    FnSwsScale swsScale = nullptr;
    FnSwsFreeContext swsFree = nullptr;
    FnSwrAllocSetOpts2 swrAllocSetOpts2 = nullptr;
    FnSwrAllocSetOpts swrAllocSetOpts = nullptr;
    FnSwrInit swrInit = nullptr;
    FnSwrConvert swrConvert = nullptr;
    FnSwrFree swrFree = nullptr;
    FnChLayoutDefault chLayoutDefault = nullptr;
    FnChLayoutUninit chLayoutUninit = nullptr;

#ifdef Q_OS_WIN
    LibHandle loadDll(const wchar_t *name)
    {
        const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromWCharArray(name));
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

    bool load()
    {
        if (avformat && avcodec && avutil && swscale)
            return true;
#ifdef Q_OS_WIN
        const QString appDir = QCoreApplication::applicationDirPath();
        SetDllDirectoryW(reinterpret_cast<LPCWSTR>(appDir.utf16()));
        loadDll(L"libwinpthread-1.dll");
        loadDll(L"zlib1.dll");
        avutil = loadDll(L"avutil-60.dll");
        swresample = loadDll(L"swresample-6.dll");
        avcodec = loadDll(L"avcodec-62.dll");
        avformat = loadDll(L"avformat-62.dll");
        swscale = loadDll(L"swscale-9.dll");
#else
        static const char *kUtil[] = {"libavutil.so", "libavutil.so.59", "libavutil.so.58", "libavutil.so.57",
                                      nullptr};
        static const char *kCodec[] = {"libavcodec.so", "libavcodec.so.61", "libavcodec.so.60", "libavcodec.so.59",
                                       "libavcodec.so.58", nullptr};
        static const char *kFmt[] = {"libavformat.so", "libavformat.so.61", "libavformat.so.60", "libavformat.so.59",
                                     "libavformat.so.58", nullptr};
        static const char *kSws[] = {"libswscale.so", "libswscale.so.8", "libswscale.so.7", "libswscale.so.6",
                                     "libswscale.so.5", nullptr};
        static const char *kSwr[] = {"libswresample.so", "libswresample.so.5", "libswresample.so.4",
                                     "libswresample.so.3", nullptr};
        for (int i = 0; kUtil[i] && !avutil; ++i)
            avutil = loadSo(kUtil[i]);
        for (int i = 0; kSwr[i] && !swresample; ++i)
            swresample = loadSo(kSwr[i]);
        for (int i = 0; kCodec[i] && !avcodec; ++i)
            avcodec = loadSo(kCodec[i]);
        for (int i = 0; kFmt[i] && !avformat; ++i)
            avformat = loadSo(kFmt[i]);
        for (int i = 0; kSws[i] && !swscale; ++i)
            swscale = loadSo(kSws[i]);
#endif
        if (!avutil || !avcodec || !avformat || !swscale)
            return false;

        networkInit = reinterpret_cast<FnNetworkInit>(ffSym(avformat, "avformat_network_init"));
        openInput = reinterpret_cast<FnOpenInput>(ffSym(avformat, "avformat_open_input"));
        findStreamInfo = reinterpret_cast<FnFindStreamInfo>(ffSym(avformat, "avformat_find_stream_info"));
        closeInput = reinterpret_cast<FnCloseInput>(ffSym(avformat, "avformat_close_input"));
        findBestStream = reinterpret_cast<FnFindBestStream>(ffSym(avformat, "av_find_best_stream"));
        readFrame = reinterpret_cast<FnReadFrame>(ffSym(avformat, "av_read_frame"));
        findDecoder = reinterpret_cast<FnFindDecoder>(ffSym(avcodec, "avcodec_find_decoder"));
        allocContext = reinterpret_cast<FnAllocContext>(ffSym(avcodec, "avcodec_alloc_context3"));
        paramsToContext = reinterpret_cast<FnParamsToContext>(ffSym(avcodec, "avcodec_parameters_to_context"));
        open2 = reinterpret_cast<FnOpen2>(ffSym(avcodec, "avcodec_open2"));
        freeContext = reinterpret_cast<FnFreeContext>(ffSym(avcodec, "avcodec_free_context"));
        sendPacket = reinterpret_cast<FnSendPacket>(ffSym(avcodec, "avcodec_send_packet"));
        receiveFrame = reinterpret_cast<FnReceiveFrame>(ffSym(avcodec, "avcodec_receive_frame"));
        packetAlloc = reinterpret_cast<FnPacketAlloc>(ffSym(avcodec, "av_packet_alloc"));
        packetFree = reinterpret_cast<FnPacketFree>(ffSym(avcodec, "av_packet_free"));
        packetUnref = reinterpret_cast<FnPacketUnref>(ffSym(avcodec, "av_packet_unref"));
        frameAlloc = reinterpret_cast<FnFrameAlloc>(ffSym(avutil, "av_frame_alloc"));
        frameFree = reinterpret_cast<FnFrameFree>(ffSym(avutil, "av_frame_free"));
        frameUnref = reinterpret_cast<FnFrameUnref>(ffSym(avutil, "av_frame_unref"));
        dictSet = reinterpret_cast<FnDictSet>(ffSym(avutil, "av_dict_set"));
        dictFree = reinterpret_cast<FnDictFree>(ffSym(avutil, "av_dict_free"));
        optGetInt = reinterpret_cast<FnOptGetInt>(ffSym(avutil, "av_opt_get_int"));
        swsGet = reinterpret_cast<FnSwsGetContext>(ffSym(swscale, "sws_getContext"));
        swsScale = reinterpret_cast<FnSwsScale>(ffSym(swscale, "sws_scale"));
        swsFree = reinterpret_cast<FnSwsFreeContext>(ffSym(swscale, "sws_freeContext"));
        if (swresample) {
            swrAllocSetOpts2 = reinterpret_cast<FnSwrAllocSetOpts2>(ffSym(swresample, "swr_alloc_set_opts2"));
            swrAllocSetOpts = reinterpret_cast<FnSwrAllocSetOpts>(ffSym(swresample, "swr_alloc_set_opts"));
            swrInit = reinterpret_cast<FnSwrInit>(ffSym(swresample, "swr_init"));
            swrConvert = reinterpret_cast<FnSwrConvert>(ffSym(swresample, "swr_convert"));
            swrFree = reinterpret_cast<FnSwrFree>(ffSym(swresample, "swr_free"));
            chLayoutDefault = reinterpret_cast<FnChLayoutDefault>(ffSym(avutil, "av_channel_layout_default"));
            chLayoutUninit = reinterpret_cast<FnChLayoutUninit>(ffSym(avutil, "av_channel_layout_uninit"));
        }

        return networkInit && openInput && findStreamInfo && closeInput && findBestStream && readFrame
            && findDecoder && allocContext && paramsToContext && open2 && freeContext && sendPacket
            && receiveFrame && packetAlloc && packetFree && packetUnref && frameAlloc && frameFree
            && frameUnref && dictSet && dictFree && swsGet && swsScale && swsFree;
    }

    int64_t optInt(void *obj, const char *key, int64_t fallback) const
    {
        if (!optGetInt || !obj)
            return fallback;
        int64_t v = 0;
        if (optGetInt(obj, key, 0, &v) < 0)
            return fallback;
        return v;
    }
};

#ifdef Q_OS_WIN
struct PcmOut {
    HWAVEOUT wave = nullptr;
    QByteArray pending;
    QMutex mutex;
    WAVEHDR headers[6]{};
    QByteArray buffers[6];
    int next = 0;

    static void CALLBACK proc(HWAVEOUT, UINT msg, DWORD_PTR user, DWORD_PTR, DWORD_PTR)
    {
        if (msg != WOM_DONE || !user)
            return;
        auto *self = reinterpret_cast<PcmOut *>(user);
        QMutexLocker lock(&self->mutex);
        self->pump();
    }

    bool start(int rate, int channels)
    {
        stop();
        if (rate <= 0)
            rate = 44100;
        if (channels <= 0)
            channels = 2;
        WAVEFORMATEX fmt{};
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = WORD(channels);
        fmt.nSamplesPerSec = DWORD(rate);
        fmt.wBitsPerSample = 16;
        fmt.nBlockAlign = WORD(channels * 2);
        fmt.nAvgBytesPerSec = DWORD(rate * fmt.nBlockAlign);
        if (waveOutOpen(&wave, WAVE_MAPPER, &fmt, DWORD_PTR(&proc), DWORD_PTR(this), CALLBACK_FUNCTION)
            != MMSYSERR_NOERROR) {
            wave = nullptr;
            return false;
        }
        for (int i = 0; i < 6; ++i) {
            buffers[i] = QByteArray(8192, 0);
            std::memset(&headers[i], 0, sizeof(WAVEHDR));
            headers[i].lpData = buffers[i].data();
            headers[i].dwBufferLength = 8192;
            waveOutPrepareHeader(wave, &headers[i], sizeof(WAVEHDR));
        }
        return true;
    }

    void write(const QByteArray &pcm)
    {
        QMutexLocker lock(&mutex);
        pending.append(pcm);
        pump();
    }

    void pump()
    {
        if (!wave)
            return;
        while (!pending.isEmpty()) {
            WAVEHDR *hdr = nullptr;
            for (int i = 0; i < 6; ++i) {
                const int idx = (next + i) % 6;
                if (headers[idx].dwFlags & WHDR_INQUEUE)
                    continue;
                hdr = &headers[idx];
                next = (idx + 1) % 6;
                break;
            }
            if (!hdr)
                return;
            const int n = qMin(8192, pending.size());
            std::memcpy(hdr->lpData, pending.constData(), size_t(n));
            pending.remove(0, n);
            hdr->dwBufferLength = DWORD(n);
            waveOutWrite(wave, hdr, sizeof(WAVEHDR));
        }
    }

    void stop()
    {
        if (!wave)
            return;
        waveOutReset(wave);
        for (int i = 0; i < 6; ++i) {
            if (headers[i].dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(wave, &headers[i], sizeof(WAVEHDR));
        }
        waveOutClose(wave);
        wave = nullptr;
        pending.clear();
    }
};
#else
struct PcmOut {
#if defined(IVI_HAVE_ALSA)
    snd_pcm_t *pcm = nullptr;
    int channels = 2;
#endif
    bool start(int rate, int channelsIn)
    {
        stop();
#if defined(IVI_HAVE_ALSA)
        if (rate <= 0)
            rate = 44100;
        if (channelsIn <= 0)
            channelsIn = 2;
        channels = channelsIn;
        if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
            pcm = nullptr;
            return false;
        }
        if (snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                               unsigned(channels), unsigned(rate), 1, 100000)
            < 0) {
            stop();
            return false;
        }
        return true;
#else
        Q_UNUSED(rate);
        Q_UNUSED(channelsIn);
        return false;
#endif
    }
    void write(const QByteArray &pcmData)
    {
#if defined(IVI_HAVE_ALSA)
        if (!pcm || pcmData.isEmpty())
            return;
        const int frameBytes = qMax(1, channels) * 2;
        snd_pcm_uframes_t left = snd_pcm_uframes_t(pcmData.size() / frameBytes);
        const char *p = pcmData.constData();
        while (left > 0) {
            const snd_pcm_sframes_t n = snd_pcm_writei(pcm, p, left);
            if (n == -EPIPE) {
                snd_pcm_prepare(pcm);
                continue;
            }
            if (n < 0)
                break;
            p += n * frameBytes;
            left -= snd_pcm_uframes_t(n);
        }
#else
        Q_UNUSED(pcmData);
#endif
    }
    void stop()
    {
#if defined(IVI_HAVE_ALSA)
        if (!pcm)
            return;
        snd_pcm_drop(pcm);
        snd_pcm_close(pcm);
        pcm = nullptr;
#endif
    }
};
#endif

AVCodecParameters *paramsOf(AVFormatContext *fmt, int index)
{
    auto *lite = reinterpret_cast<AVFormatLite *>(fmt);
    if (!lite || !lite->streams || index < 0 || unsigned(index) >= lite->nb_streams)
        return nullptr;
    return lite->streams[index]->codecpar;
}

int codecIdOf(AVCodecParameters *par)
{
    // codec_type(int) + codec_id(int)
    return reinterpret_cast<const int *>(par)[1];
}

#endif

} // namespace

FfmpegUrlPlayer::FfmpegUrlPlayer(QObject *parent)
    : QObject(parent)
{
}

FfmpegUrlPlayer::~FfmpegUrlPlayer()
{
    stop();
    while (m_workerBusy.load())
        QThread::msleep(10);
}

QImage FfmpegUrlPlayer::currentFrame() const
{
    QMutexLocker lock(&m_frameMutex);
    return m_frame;
}

QString FfmpegUrlPlayer::lastError() const
{
    return m_error;
}

void FfmpegUrlPlayer::play(const QUrl &url)
{
    stop();
    while (m_workerBusy.load())
        QThread::msleep(10);
    m_stop.store(false);
    m_paused.store(false);
    m_error.clear();
    {
        QMutexLocker lock(&m_frameMutex);
        m_frame = QImage();
    }
    const QString u = url.toString();
    m_workerBusy.store(true);
    setPlaying(true);
    QThreadPool::globalInstance()->start([this, u] {
        runLoop(u);
        m_workerBusy.store(false);
        setPlaying(false);
        emit finished();
    });
}

void FfmpegUrlPlayer::pause()
{
    m_paused.store(true);
}

void FfmpegUrlPlayer::resume()
{
    m_paused.store(false);
}

void FfmpegUrlPlayer::stop()
{
    m_stop.store(true);
    m_paused.store(false);
}

void FfmpegUrlPlayer::setPlaying(bool on)
{
    const bool prev = m_playing.exchange(on);
    if (prev != on)
        emit playingChanged();
}

void FfmpegUrlPlayer::pushFrame(const QImage &img)
{
    {
        QMutexLocker lock(&m_frameMutex);
        m_frame = img;
    }
    emit frameChanged();
}

void FfmpegUrlPlayer::runLoop(const QString &url)
{
#ifdef IVI_FFMPEG_DYN
    Libs lib;
    if (!lib.load()) {
        m_error = QStringLiteral("缺少 FFmpeg（avformat/avcodec/swscale）");
        emit errorOccurred(m_error);
        return;
    }
    lib.networkInit();

    AVFormatContext *fmt = nullptr;
    AVDictionary *opts = nullptr;
    lib.dictSet(&opts, "stimeout", "8000000", 0);
    lib.dictSet(&opts, "rw_timeout", "8000000", 0);
    lib.dictSet(&opts, "user_agent", "MP157-IVI-DLNA/1.0", 0);
    if (lib.openInput(&fmt, url.toUtf8().constData(), nullptr, &opts) < 0 || !fmt) {
        lib.dictFree(&opts);
        m_error = QStringLiteral("打开媒体失败");
        emit errorOccurred(m_error);
        return;
    }
    lib.dictFree(&opts);
    if (lib.findStreamInfo(fmt, nullptr) < 0) {
        m_error = QStringLiteral("解析媒体失败");
        emit errorOccurred(m_error);
        lib.closeInput(&fmt);
        return;
    }

    const AVCodec *vCodec = nullptr;
    const AVCodec *aCodec = nullptr;
    const int vIdx = lib.findBestStream(fmt, kAvMediaTypeVideo, -1, -1, &vCodec, 0);
    const int aIdx = lib.findBestStream(fmt, kAvMediaTypeAudio, -1, -1, &aCodec, 0);

    AVCodecContext *vCtx = nullptr;
    AVCodecContext *aCtx = nullptr;
    if (vIdx >= 0) {
        if (!vCodec)
            vCodec = lib.findDecoder(codecIdOf(paramsOf(fmt, vIdx)));
        vCtx = lib.allocContext(vCodec);
        AVCodecParameters *par = paramsOf(fmt, vIdx);
        if (!vCodec || !vCtx || !par || lib.paramsToContext(vCtx, par) < 0 || lib.open2(vCtx, vCodec, nullptr) < 0) {
            if (vCtx)
                lib.freeContext(&vCtx);
            vCtx = nullptr;
        }
    }
    if (aIdx >= 0) {
        if (!aCodec)
            aCodec = lib.findDecoder(codecIdOf(paramsOf(fmt, aIdx)));
        aCtx = lib.allocContext(aCodec);
        AVCodecParameters *par = paramsOf(fmt, aIdx);
        if (!aCodec || !aCtx || !par || lib.paramsToContext(aCtx, par) < 0 || lib.open2(aCtx, aCodec, nullptr) < 0) {
            if (aCtx)
                lib.freeContext(&aCtx);
            aCtx = nullptr;
        }
    }
    if (!vCtx && !aCtx) {
        m_error = QStringLiteral("无可用音视频流");
        emit errorOccurred(m_error);
        lib.closeInput(&fmt);
        return;
    }

    AVPacket *pkt = lib.packetAlloc();
    AVFrame *frame = lib.frameAlloc();
    SwsContext *sws = nullptr;
    SwrContext *swr = nullptr;
    PcmOut audio;
    QByteArray rgb;
    int rgbW = 0;
    int rgbH = 0;
    const int outCh = 2;
    int outRate = int(lib.optInt(aCtx, "sample_rate", 44100));
    if (outRate <= 0)
        outRate = 44100;
    bool audioStarted = false;

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

    auto ensureSwr = [&](int inFmt, int inRate, int inCh) {
        if (swr)
            return true;
        if (inRate <= 0)
            inRate = outRate;
        if (inCh <= 0)
            inCh = outCh;
        constexpr int64_t kStereo = 3; // AV_CH_LAYOUT_STEREO
        if (lib.swrAllocSetOpts2 && lib.chLayoutDefault) {
            AVChannelLayout outLayout{};
            AVChannelLayout inLayout{};
            lib.chLayoutDefault(&outLayout, outCh);
            lib.chLayoutDefault(&inLayout, inCh);
            if (lib.swrAllocSetOpts2(&swr, &outLayout, kAvSampleFmtS16, outRate, &inLayout, inFmt, inRate, 0,
                                     nullptr)
                < 0) {
                swr = nullptr;
            } else if (lib.swrInit(swr) < 0) {
                lib.swrFree(&swr);
                swr = nullptr;
            }
            if (lib.chLayoutUninit) {
                lib.chLayoutUninit(&outLayout);
                lib.chLayoutUninit(&inLayout);
            }
        }
        if (!swr && lib.swrAllocSetOpts) {
            swr = lib.swrAllocSetOpts(nullptr, kStereo, kAvSampleFmtS16, outRate, kStereo, inFmt, inRate, 0,
                                      nullptr);
            if (swr && lib.swrInit(swr) < 0) {
                lib.swrFree(&swr);
                swr = nullptr;
            }
        }
        if (!swr || !lib.swrConvert)
            return false;
        if (!audioStarted)
            audioStarted = audio.start(outRate, outCh);
        return audioStarted;
    };

    while (!m_stop.load()) {
        while (m_paused.load() && !m_stop.load())
            QThread::msleep(30);
        if (m_stop.load())
            break;

        lib.packetUnref(pkt);
        const int rr = lib.readFrame(fmt, pkt);
        if (rr == kAverrorEof || rr < 0)
            break;
        const int streamIndex = reinterpret_cast<AVPacketLite *>(pkt)->stream_index;

        if (vCtx && streamIndex == vIdx) {
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
        } else if (aCtx && streamIndex == aIdx) {
            if (lib.sendPacket(aCtx, pkt) < 0)
                continue;
            while (!m_stop.load()) {
                lib.frameUnref(frame);
                const int got = lib.receiveFrame(aCtx, frame);
                if (got == kAverrorEagain || got == kAverrorEof)
                    break;
                if (got < 0)
                    break;
                auto *fr = reinterpret_cast<AVFrameLite *>(frame);
                if (fr->nb_samples <= 0)
                    continue;
                const int inCh = int(lib.optInt(aCtx, "channels", outCh));
                if (!ensureSwr(fr->format, outRate, inCh > 0 ? inCh : outCh))
                    continue;
                const int maxOut = fr->nb_samples * 4 + 256;
                QByteArray pcm(maxOut * outCh * 2, 0);
                uint8_t *outPtr = reinterpret_cast<uint8_t *>(pcm.data());
                const uint8_t **inPtr = const_cast<const uint8_t **>(
                    fr->extended_data ? fr->extended_data : fr->data);
                const int converted = lib.swrConvert(swr, &outPtr, maxOut, inPtr, fr->nb_samples);
                if (converted > 0) {
                    pcm.resize(converted * outCh * 2);
                    audio.write(pcm);
                }
            }
        }
    }

    audio.stop();
    if (swr && lib.swrFree)
        lib.swrFree(&swr);
    if (sws)
        lib.swsFree(sws);
    if (frame)
        lib.frameFree(&frame);
    if (pkt)
        lib.packetFree(&pkt);
    if (aCtx)
        lib.freeContext(&aCtx);
    if (vCtx)
        lib.freeContext(&vCtx);
    lib.closeInput(&fmt);
#else
    Q_UNUSED(url);
    m_error = QStringLiteral("当前平台未接 FFmpeg 播放");
    emit errorOccurred(m_error);
#endif
}

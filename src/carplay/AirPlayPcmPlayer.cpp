#include "AirPlayPcmPlayer.hpp"

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

#include <QVector>
#include <cstring>

struct AirPlayPcmPlayer::Impl {
#ifdef Q_OS_WIN
    HWAVEOUT wave = nullptr;
    QMutex mutex;
    QByteArray pending;
    QVector<WAVEHDR> headers;
    QVector<QByteArray> buffers;
    int next = 0;
    static constexpr int kBuffers = 8;
    static constexpr int kBufferBytes = 8192;

    static void CALLBACK waveProc(HWAVEOUT, UINT msg, DWORD_PTR user, DWORD_PTR, DWORD_PTR)
    {
        if (msg != WOM_DONE || !user)
            return;
        auto *self = reinterpret_cast<Impl *>(user);
        QMutexLocker lock(&self->mutex);
        self->pumpLocked();
    }

    void pumpLocked()
    {
        if (!wave)
            return;
        while (true) {
            WAVEHDR *hdr = nullptr;
            for (int i = 0; i < headers.size(); ++i) {
                const int idx = (next + i) % headers.size();
                if (headers[idx].dwFlags & WHDR_INQUEUE)
                    continue;
                if (!(headers[idx].dwFlags & WHDR_PREPARED))
                    continue;
                hdr = &headers[idx];
                next = (idx + 1) % headers.size();
                break;
            }
            if (!hdr)
                return;
            if (pending.isEmpty())
                return;
            const int n = qMin(kBufferBytes, pending.size());
            std::memcpy(hdr->lpData, pending.constData(), size_t(n));
            pending.remove(0, n);
            hdr->dwBufferLength = DWORD(n);
            waveOutWrite(wave, hdr, sizeof(WAVEHDR));
        }
    }
#endif
};

AirPlayPcmPlayer::AirPlayPcmPlayer(QObject *parent)
    : QObject(parent)
    , m(new Impl)
{
}

AirPlayPcmPlayer::~AirPlayPcmPlayer()
{
    stop();
    delete m;
}

bool AirPlayPcmPlayer::start(int sampleRate, int channels)
{
    stop();
#ifdef Q_OS_WIN
    if (sampleRate <= 0)
        sampleRate = 44100;
    if (channels <= 0)
        channels = 2;
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = WORD(channels);
    fmt.nSamplesPerSec = DWORD(sampleRate);
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = WORD(channels * 2);
    fmt.nAvgBytesPerSec = DWORD(sampleRate * fmt.nBlockAlign);
    if (waveOutOpen(&m->wave, WAVE_MAPPER, &fmt, DWORD_PTR(&Impl::waveProc), DWORD_PTR(m),
                    CALLBACK_FUNCTION)
        != MMSYSERR_NOERROR) {
        m->wave = nullptr;
        return false;
    }
    m->headers.resize(Impl::kBuffers);
    m->buffers.resize(Impl::kBuffers);
    for (int i = 0; i < Impl::kBuffers; ++i) {
        m->buffers[i] = QByteArray(Impl::kBufferBytes, 0);
        WAVEHDR &hdr = m->headers[i];
        std::memset(&hdr, 0, sizeof(hdr));
        hdr.lpData = m->buffers[i].data();
        hdr.dwBufferLength = Impl::kBufferBytes;
        waveOutPrepareHeader(m->wave, &hdr, sizeof(WAVEHDR));
    }
    m_running = true;
    return true;
#else
    Q_UNUSED(sampleRate);
    Q_UNUSED(channels);
    return false;
#endif
}

void AirPlayPcmPlayer::stop()
{
#ifdef Q_OS_WIN
    m_running = false;
    if (!m->wave)
        return;
    waveOutReset(m->wave);
    for (WAVEHDR &hdr : m->headers) {
        if (hdr.dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(m->wave, &hdr, sizeof(WAVEHDR));
    }
    waveOutClose(m->wave);
    m->wave = nullptr;
    m->headers.clear();
    m->buffers.clear();
    m->pending.clear();
    m->next = 0;
#endif
}

void AirPlayPcmPlayer::writePcm(const QByteArray &pcm)
{
#ifdef Q_OS_WIN
    if (!m_running || !m->wave || pcm.isEmpty())
        return;
    QMutexLocker lock(&m->mutex);
    if (m->pending.size() > 512 * 1024)
        m->pending.remove(0, m->pending.size() - 256 * 1024);
    m->pending.append(pcm);
    m->pumpLocked();
#else
    Q_UNUSED(pcm);
#endif
}

#include "MediaSession.hpp"

#include "AudioFocus.hpp"
#include "SystemState.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QVariantMap>
#include <QtEndian>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace {

int wavSeconds(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return 1;
    const QByteArray data = file.read(65536);
    if (data.size() < 44)
        return 1;
    int rate = 44100;
    int channels = 1;
    int bits = 16;
    int dataSize = 0;
    int i = 12;
    while (i + 8 <= data.size()) {
        const QByteArray id = data.mid(i, 4);
        const quint32 size = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + i + 4));
        if (id == "fmt " && i + 24 <= data.size()) {
            channels = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(data.constData() + i + 10));
            rate = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + i + 12));
            bits = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(data.constData() + i + 22));
        } else if (id == "data") {
            dataSize = int(size);
            break;
        }
        i += 8 + int(size) + int(size & 1);
    }
    const int bytesPerSecond = rate * channels * qMax(1, bits / 8);
    if (bytesPerSecond <= 0 || dataSize <= 0)
        return 1;
    return qMax(1, dataSize / bytesPerSecond);
}

#ifdef Q_OS_WIN
void mci(const QString &command)
{
    mciSendStringW(reinterpret_cast<const wchar_t *>(command.utf16()), nullptr, 0, nullptr);
}

QString mciStatus(const wchar_t *command)
{
    wchar_t buffer[64] = {};
    mciSendStringW(command, buffer, 64, nullptr);
    return QString::fromWCharArray(buffer);
}
#endif

}

MediaSession::MediaSession(SystemState *system, AudioFocus *audio, QObject *parent)
    : QObject(parent)
    , m_system(system)
    , m_audio(audio)
{
    QTimer::singleShot(0, this, [this] {
        loadTracks();
        updateSource();
    });
    connect(m_system, &SystemState::bluetoothChanged, this, &MediaSession::updateSource);
    connect(m_system, &SystemState::volumeChanged, this, &MediaSession::applyVolume);
    connect(m_audio, &AudioFocus::duckedChanged, this, &MediaSession::applyVolume);
    m_timer.setInterval(500);
    connect(&m_timer, &QTimer::timeout, this, &MediaSession::poll);
}

MediaSession::~MediaSession()
{
#ifdef Q_OS_WIN
    mci(QStringLiteral("close iviMusic"));
#endif
}

bool MediaSession::playing() const
{
    return m_playing;
}

QString MediaSession::source() const
{
    return m_source;
}

QString MediaSession::title() const
{
    if (m_tracks.isEmpty())
        return {};
    return m_tracks.at(m_index).title;
}

QString MediaSession::artist() const
{
    if (m_tracks.isEmpty())
        return {};
    return m_tracks.at(m_index).artist;
}

int MediaSession::trackIndex() const
{
    return m_index;
}

int MediaSession::position() const
{
    return m_position;
}

int MediaSession::duration() const
{
    if (m_tracks.isEmpty())
        return 1;
    return m_tracks.at(m_index).duration;
}

QVariantList MediaSession::tracks() const
{
    QVariantList list;
    for (const Track &track : m_tracks) {
        QVariantMap map;
        map.insert(QStringLiteral("title"), track.title);
        map.insert(QStringLiteral("artist"), track.artist);
        map.insert(QStringLiteral("duration"), track.duration);
        list.push_back(map);
    }
    return list;
}

void MediaSession::play()
{
    if (m_tracks.isEmpty())
        return;
    if (!m_playing) {
        m_playing = true;
        emit playingChanged();
    }
    m_audio->request(QStringLiteral("media"), m_audio->mediaPriority());
#ifdef Q_OS_WIN
    if (m_tracks.isEmpty())
        return;
    if (mciStatus(L"status iviMusic mode") == QStringLiteral(""))
        openCurrent();
    if (mciStatus(L"status iviMusic mode") != QStringLiteral("playing"))
        mci(QStringLiteral("play iviMusic"));
#endif
    if (!m_timer.isActive())
        m_timer.start();
    applyVolume();
}

void MediaSession::pause()
{
#ifdef Q_OS_WIN
    mci(QStringLiteral("pause iviMusic"));
#endif
    m_audio->release(QStringLiteral("media"));
    if (!m_playing)
        return;
    m_playing = false;
    m_timer.stop();
    emit playingChanged();
}

void MediaSession::toggle()
{
    if (m_playing)
        pause();
    else
        play();
}

void MediaSession::next()
{
    if (m_tracks.isEmpty())
        return;
    select((m_index + 1) % m_tracks.size(), true);
}

void MediaSession::previous()
{
    if (m_tracks.isEmpty())
        return;
    if (m_position > 3) {
        seek(0);
        play();
        return;
    }
    select((m_index - 1 + m_tracks.size()) % m_tracks.size(), true);
}

void MediaSession::playIndex(int index)
{
    select(index, true);
}

void MediaSession::seek(int seconds)
{
    m_position = qBound(0, seconds, qMax(0, duration() - 1));
    emit positionChanged();
#ifdef Q_OS_WIN
    mci(QStringLiteral("seek iviMusic to %1").arg(m_position * 1000));
    if (m_playing)
        mci(QStringLiteral("play iviMusic"));
#endif
}

void MediaSession::loadTracks()
{
    const QStringList titles = {
        QStringLiteral("夜路"),
        QStringLiteral("城市灯火"),
        QStringLiteral("回程"),
        QStringLiteral("晴空"),
        QStringLiteral("江岸")
    };
    const QDir dir(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("media/music")));
    for (const QString &title : titles) {
        const QString path = dir.filePath(title + QStringLiteral(".wav"));
        if (!QFile::exists(path))
            continue;
        Track track;
        track.title = title;
        track.artist = QStringLiteral("示例");
        track.file = path;
        track.duration = wavSeconds(path);
        m_tracks.push_back(track);
    }
}

void MediaSession::openCurrent()
{
#ifdef Q_OS_WIN
    mci(QStringLiteral("close iviMusic"));
    if (m_tracks.isEmpty())
        return;
    mci(QStringLiteral("open \"%1\" type waveaudio alias iviMusic").arg(m_tracks.at(m_index).file));
    applyVolume();
#endif
}

void MediaSession::applyVolume()
{
#ifdef Q_OS_WIN
    const qreal gain = m_audio->ducked() ? 0.25 : 1.0;
    const int volume = qBound(0, qRound(m_system->volume() * gain * 1000.0), 1000);
    mci(QStringLiteral("setaudio iviMusic volume to %1").arg(volume));
#endif
}

void MediaSession::updateSource()
{
    const QString source = m_system->bluetooth() ? QStringLiteral("蓝牙音频") : QStringLiteral("本机");
    if (source == m_source)
        return;
    m_source = source;
    emit sourceChanged();
}

void MediaSession::poll()
{
#ifdef Q_OS_WIN
    if (!m_playing || m_tracks.isEmpty() || m_advancing)
        return;
    const int ms = mciStatus(L"status iviMusic position").toInt();
    const int seconds = ms / 1000;
    if (seconds != m_position) {
        m_position = seconds;
        emit positionChanged();
    }
    const int length = mciStatus(L"status iviMusic length").toInt();
    if (mciStatus(L"status iviMusic mode") == QStringLiteral("stopped") && length > 0 && ms + 400 >= length) {
        m_advancing = true;
        next();
        m_advancing = false;
    }
#else
    Q_UNUSED(this);
#endif
}

void MediaSession::select(int index, bool start)
{
    if (index < 0 || index >= m_tracks.size() || m_advancing && index == m_index)
        return;
    m_index = index;
    m_position = 0;
    emit trackChanged();
    emit positionChanged();
    openCurrent();
    if (start)
        play();
}

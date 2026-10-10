#include "MediaSession.hpp"

#include "AudioFocus.hpp"
#include "SystemState.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRandomGenerator>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QVariantMap>
#include <QtEndian>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#elif defined(IVI_HAVE_ALSA)
#include <alsa/asoundlib.h>
#endif

namespace {

const QStringList kColors = {
    QStringLiteral("#FF2D55"),
    QStringLiteral("#5856D6"),
    QStringLiteral("#007AFF"),
    QStringLiteral("#34C759"),
    QStringLiteral("#FF9500"),
    QStringLiteral("#AF52DE"),
    QStringLiteral("#FF3B30"),
};

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
#elif defined(IVI_HAVE_ALSA)
struct AlsaMusic {
    snd_pcm_t *pcm = nullptr;
    QByteArray data;
    int rate = 44100;
    int channels = 2;
    int offset = 0;
    qreal volume = 1.0;

    void closeDevice()
    {
        if (!pcm)
            return;
        snd_pcm_drop(pcm);
        snd_pcm_close(pcm);
        pcm = nullptr;
    }

    void clear()
    {
        closeDevice();
        data.clear();
        offset = 0;
    }

    bool loadWav(const QString &path)
    {
        clear();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        const QByteArray all = file.readAll();
        if (all.size() < 44)
            return false;
        int bits = 16;
        int dataSize = 0;
        int dataPos = -1;
        int i = 12;
        while (i + 8 <= all.size()) {
            const QByteArray id = all.mid(i, 4);
            const quint32 size = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(all.constData() + i + 4));
            if (id == "fmt " && i + 24 <= all.size()) {
                channels = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(all.constData() + i + 10));
                rate = int(qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(all.constData() + i + 12)));
                bits = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(all.constData() + i + 22));
            } else if (id == "data") {
                dataPos = i + 8;
                dataSize = int(size);
                break;
            }
            i += 8 + int(size) + int(size & 1);
        }
        if (dataPos < 0 || bits != 16 || channels <= 0 || rate <= 0)
            return false;
        data = all.mid(dataPos, qMin(dataSize, all.size() - dataPos));
        offset = 0;
        if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
            pcm = nullptr;
            return false;
        }
        if (snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                               unsigned(channels), unsigned(rate), 1, 100000)
            < 0) {
            closeDevice();
            return false;
        }
        return true;
    }

    void seekMs(int ms)
    {
        const int frameBytes = qMax(1, channels) * 2;
        const qint64 byte = qint64(ms) * rate * frameBytes / 1000;
        offset = int(qBound(qint64(0), byte - (byte % frameBytes), qint64(data.size())));
        if (pcm)
            snd_pcm_drop(pcm), snd_pcm_prepare(pcm);
    }

    void writeMs(int ms)
    {
        if (!pcm || data.isEmpty() || offset >= data.size())
            return;
        const int frameBytes = qMax(1, channels) * 2;
        int bytes = rate * frameBytes * ms / 1000;
        bytes -= bytes % frameBytes;
        bytes = qMin(bytes, data.size() - offset);
        if (bytes <= 0)
            return;
        QByteArray chunk = data.mid(offset, bytes);
        if (volume < 0.999) {
            auto *s = reinterpret_cast<qint16 *>(chunk.data());
            const int n = chunk.size() / 2;
            for (int i = 0; i < n; ++i)
                s[i] = qint16(qBound(-32768, int(s[i] * volume), 32767));
        }
        const char *p = chunk.constData();
        snd_pcm_uframes_t left = snd_pcm_uframes_t(bytes / frameBytes);
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
        offset += bytes;
    }

    bool ended() const { return !data.isEmpty() && offset >= data.size(); }
};

AlsaMusic g_alsaMusic;
#endif

}

MediaSession::MediaSession(SystemState *system, AudioFocus *audio, QObject *parent)
    : QObject(parent)
    , m_system(system)
    , m_audio(audio)
{
    QTimer::singleShot(0, this, [this] {
        loadTracks();
        loadPrefs();
        ensureQueue();
        updateSource();
    });
    connect(m_system, &SystemState::bluetoothChanged, this, &MediaSession::updateSource);
    connect(m_system, &SystemState::volumeChanged, this, &MediaSession::applyVolume);
    connect(m_audio, &AudioFocus::duckedChanged, this, &MediaSession::applyVolume);
    connect(m_audio, &AudioFocus::ownerChanged, this, [this] {
        if (!m_playing || m_remoteActive)
            return;
        const QString owner = m_audio->owner();
        if (owner.isEmpty() || owner == QLatin1String("media"))
            return;
        pause();
    });
    m_timer.setInterval(500);
    connect(&m_timer, &QTimer::timeout, this, &MediaSession::poll);
}

MediaSession::~MediaSession()
{
#ifdef Q_OS_WIN
    mci(QStringLiteral("close iviMusic"));
#elif defined(IVI_HAVE_ALSA)
    g_alsaMusic.clear();
#endif
}

bool MediaSession::playing() const
{
    return m_remoteActive ? m_remotePlaying : m_playing;
}

bool MediaSession::bluetoothMode() const
{
    return m_btActive && !m_remoteActive;
}

void MediaSession::setBluetoothSource(const QString &name, bool active)
{
    const bool was = m_btActive;
    m_btActive = active;
    m_btName = name;
    updateSource();
    if (active && !was && m_playing && !m_remoteActive)
        pause();
}

QString MediaSession::source() const
{
    if (m_remoteActive)
        return QStringLiteral("carplay");
    if (m_btActive)
        return m_btName.isEmpty() ? QStringLiteral("蓝牙") : m_btName;
    return m_source;
}

QString MediaSession::title() const
{
    if (m_remoteActive)
        return m_remoteTitle;
    if (m_tracks.isEmpty())
        return {};
    return m_tracks.at(m_index).title;
}

QString MediaSession::artist() const
{
    if (m_remoteActive)
        return m_remoteArtist;
    if (m_tracks.isEmpty())
        return {};
    return m_tracks.at(m_index).artist;
}

QString MediaSession::coverColor() const
{
    if (m_tracks.isEmpty())
        return QStringLiteral("#FF2D55");
    return m_tracks.at(m_index).color;
}

int MediaSession::trackIndex() const
{
    return m_index;
}

int MediaSession::position() const
{
    return m_remoteActive ? m_remotePosition : m_position;
}

int MediaSession::duration() const
{
    if (m_remoteActive)
        return qMax(1, m_remoteDuration);
    if (m_tracks.isEmpty())
        return 1;
    return m_tracks.at(m_index).duration;
}

void MediaSession::applyRemoteNowPlaying(const QString &title, const QString &artist, bool playing,
                                         int positionSec, int durationSec)
{
    const bool wasPlaying = this->playing();
    m_remoteActive = true;
    m_remoteTitle = title;
    m_remoteArtist = artist;
    m_remotePlaying = playing;
    m_remotePosition = qMax(0, positionSec);
    m_remoteDuration = qMax(1, durationSec);
    emit trackChanged();
    emit positionChanged();
    emit sourceChanged();
    if (wasPlaying != playing)
        emit playingChanged();
}

void MediaSession::clearRemoteNowPlaying()
{
    if (!m_remoteActive)
        return;
    const bool wasPlaying = playing();
    m_remoteActive = false;
    m_remotePlaying = false;
    m_remoteTitle.clear();
    m_remoteArtist.clear();
    emit trackChanged();
    emit positionChanged();
    emit sourceChanged();
    if (wasPlaying != m_playing)
        emit playingChanged();
}

int MediaSession::playMode() const
{
    return m_playMode;
}

QVariantList MediaSession::tracks() const
{
    QVariantList list;
    for (const Track &track : m_tracks) {
        QVariantMap map;
        map.insert(QStringLiteral("title"), track.title);
        map.insert(QStringLiteral("artist"), track.artist);
        map.insert(QStringLiteral("duration"), track.duration);
        map.insert(QStringLiteral("color"), track.color);
        list.push_back(map);
    }
    return list;
}

QVariantList MediaSession::queue() const
{
    QVariantList list;
    for (int index : m_queue)
        list.push_back(index);
    return list;
}

QStringList MediaSession::lyrics() const
{
    if (m_tracks.isEmpty())
        return {};
    return m_tracks.at(m_index).lyrics;
}

void MediaSession::play()
{
    if (m_tracks.isEmpty())
        return;
    if (!m_playing) {
        m_playing = true;
        emit playingChanged();
    }
    const int prio = (m_btActive && !m_remoteActive) ? m_audio->btPriority() : m_audio->mediaPriority();
    m_audio->request(QStringLiteral("media"), prio);
#ifdef Q_OS_WIN
    if (mciStatus(L"status iviMusic mode") == QStringLiteral(""))
        openCurrent();
    if (mciStatus(L"status iviMusic mode") != QStringLiteral("playing"))
        mci(QStringLiteral("play iviMusic"));
#elif defined(IVI_HAVE_ALSA)
    if (g_alsaMusic.data.isEmpty())
        openCurrent();
    g_alsaMusic.seekMs(m_position * 1000);
    m_softMs = m_position * 1000;
#else
    m_softMs = m_position * 1000;
#endif
    if (!m_timer.isActive())
        m_timer.start();
    applyVolume();
}

void MediaSession::pause()
{
    if (!m_playing) {
        m_audio->release(QStringLiteral("media"));
        return;
    }
#ifdef Q_OS_WIN
    mci(QStringLiteral("pause iviMusic"));
#elif defined(IVI_HAVE_ALSA)
    if (g_alsaMusic.pcm)
        snd_pcm_drop(g_alsaMusic.pcm), snd_pcm_prepare(g_alsaMusic.pcm);
#endif
    m_playing = false;
    m_timer.stop();
    emit playingChanged();
    m_audio->release(QStringLiteral("media"));
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
    advance(1, false);
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
    advance(-1, false);
}

void MediaSession::playIndex(int index)
{
    ensureQueue();
    select(index, true);
}

bool MediaSession::playFile(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile())
        return false;
    if (info.suffix().compare(QLatin1String("wav"), Qt::CaseInsensitive) != 0)
        return false;
    const QString abs = info.absoluteFilePath();
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (QFileInfo(m_tracks.at(i).file).absoluteFilePath() == abs) {
            playIndex(i);
            return true;
        }
    }
    Track track;
    track.title = info.completeBaseName();
    track.artist = QStringLiteral("本机");
    track.file = abs;
    track.color = kColors.at(m_tracks.size() % kColors.size());
    track.duration = wavSeconds(track.file);
    track.lyrics = loadLyrics(track.file, track.title);
    m_tracks.push_back(track);
    m_queue.clear();
    ensureQueue();
    emit tracksChanged();
    playIndex(m_tracks.size() - 1);
    return true;
}

void MediaSession::seek(int seconds)
{
    m_position = qBound(0, seconds, qMax(0, duration() - 1));
    m_softMs = m_position * 1000;
    emit positionChanged();
#ifdef Q_OS_WIN
    mci(QStringLiteral("seek iviMusic to %1").arg(m_position * 1000));
    if (m_playing)
        mci(QStringLiteral("play iviMusic"));
#elif defined(IVI_HAVE_ALSA)
    g_alsaMusic.seekMs(m_position * 1000);
#endif
}

void MediaSession::setQueue(const QVariantList &indices)
{
    QVector<int> queue;
    for (const QVariant &value : indices) {
        const int index = value.toInt();
        if (index >= 0 && index < m_tracks.size() && !queue.contains(index))
            queue.push_back(index);
    }
    if (queue.isEmpty()) {
        for (int i = 0; i < m_tracks.size(); ++i)
            queue.push_back(i);
    }
    m_queue = queue;
    rebuildShuffle();
    emit queueChanged();
    savePrefs();
}

void MediaSession::playAll()
{
    ensureQueue();
    if (m_queue.isEmpty())
        return;
    if (m_playMode == Shuffle) {
        rebuildShuffle();
        select(m_shuffle.first(), true);
        return;
    }
    select(m_queue.first(), true);
}

void MediaSession::playInQueue(int trackIndex)
{
    ensureQueue();
    if (!m_queue.contains(trackIndex)) {
        QVariantList list = queue();
        list.push_back(trackIndex);
        setQueue(list);
    }
    select(trackIndex, true);
}

void MediaSession::cyclePlayMode()
{
    m_playMode = (m_playMode + 1) % 3;
    if (m_playMode == Shuffle)
        rebuildShuffle();
    emit playModeChanged();
    savePrefs();
}

void MediaSession::rescan()
{
    const bool wasPlaying = m_playing;
    pause();
    m_tracks.clear();
    m_queue.clear();
    m_shuffle.clear();
    m_index = 0;
    m_position = 0;
    loadTracks();
    ensureQueue();
    emit tracksChanged();
    emit trackChanged();
    emit positionChanged();
    emit queueChanged();
    if (wasPlaying && !m_tracks.isEmpty())
        play();
}

QStringList MediaSession::loadLyrics(const QString &wavPath, const QString &title)
{
    const QFileInfo info(wavPath);
    const QStringList candidates = {
        info.absolutePath() + QLatin1Char('/') + info.completeBaseName() + QStringLiteral(".lrc"),
        info.absolutePath() + QLatin1Char('/') + info.completeBaseName() + QStringLiteral(".txt")
    };
    for (const QString &path : candidates) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        QStringList lines;
        QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        stream.setCodec("UTF-8");
#endif
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.isEmpty())
                continue;
            const int close = line.indexOf(QLatin1Char(']'));
            if (line.startsWith(QLatin1Char('[')) && close > 0)
                line = line.mid(close + 1).trimmed();
            if (!line.isEmpty())
                lines.push_back(line);
        }
        if (!lines.isEmpty())
            return lines;
    }

    static const QHash<QString, QStringList> kFallback = {
        {QStringLiteral("夜路"), {QStringLiteral("这条夜路没有灯"), QStringLiteral("只有车灯往前"), QStringLiteral("远光切开浓雾"), QStringLiteral("把回家的方向照亮")}},
        {QStringLiteral("城市灯火"), {QStringLiteral("城市灯火一盏盏"), QStringLiteral("都落在车窗上"), QStringLiteral("红灯把影子拉长"), QStringLiteral("夜色慢慢亮起来")}},
        {QStringLiteral("回程"), {QStringLiteral("回程的路变短了"), QStringLiteral("歌还在单曲循环"), QStringLiteral("电台只剩沙沙声"), QStringLiteral("油表还剩一半")}},
        {QStringLiteral("晴空"), {QStringLiteral("晴空把影子拉长"), QStringLiteral("风从侧窗进来"), QStringLiteral("云缝漏下一束光"), QStringLiteral("公路没有尽头")}},
        {QStringLiteral("江岸"), {QStringLiteral("江岸的风很轻"), QStringLiteral("把后视镜吹凉"), QStringLiteral("水纹推着旧时光"), QStringLiteral("晚霞落在方向盘")}},
    };
    return kFallback.value(title, {title});
}

void MediaSession::loadTracks()
{
    const QDir dir(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("media/music")));
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.wav"), QStringLiteral("*.WAV")}, QDir::Files, QDir::Name);
    int i = 0;
    for (const QFileInfo &info : files) {
        Track track;
        track.title = info.completeBaseName();
        track.artist = QStringLiteral("本机");
        track.file = info.absoluteFilePath();
        track.color = kColors.at(i % kColors.size());
        track.duration = wavSeconds(track.file);
        track.lyrics = loadLyrics(track.file, track.title);
        m_tracks.push_back(track);
        ++i;
    }
    emit tracksChanged();
}

void MediaSession::loadPrefs()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("media"));
    m_playMode = qBound(0, settings.value(QStringLiteral("playMode"), Loop).toInt(), 2);
    m_index = qBound(0, settings.value(QStringLiteral("trackIndex"), 0).toInt(), qMax(0, m_tracks.size() - 1));
    const QVariantList savedQueue = settings.value(QStringLiteral("queue")).toList();
    settings.endGroup();
    if (!savedQueue.isEmpty())
        setQueue(savedQueue);
    emit playModeChanged();
    emit trackChanged();
}

void MediaSession::savePrefs() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("media"));
    settings.setValue(QStringLiteral("playMode"), m_playMode);
    settings.setValue(QStringLiteral("trackIndex"), m_index);
    settings.setValue(QStringLiteral("queue"), queue());
    settings.endGroup();
}

void MediaSession::openCurrent()
{
#ifdef Q_OS_WIN
    mci(QStringLiteral("close iviMusic"));
    if (m_tracks.isEmpty())
        return;
    mci(QStringLiteral("open \"%1\" type waveaudio alias iviMusic").arg(m_tracks.at(m_index).file));
    applyVolume();
#elif defined(IVI_HAVE_ALSA)
    m_softMs = 0;
    if (m_tracks.isEmpty())
        return;
    g_alsaMusic.loadWav(m_tracks.at(m_index).file);
    applyVolume();
#else
    m_softMs = 0;
#endif
}

void MediaSession::applyVolume()
{
#ifdef Q_OS_WIN
    const qreal gain = m_audio->ducked() ? 0.25 : 1.0;
    const int volume = qBound(0, qRound(m_system->volume() * gain * 1000.0), 1000);
    mci(QStringLiteral("setaudio iviMusic volume to %1").arg(volume));
#elif defined(IVI_HAVE_ALSA)
    const qreal gain = m_audio->ducked() ? 0.25 : 1.0;
    g_alsaMusic.volume = m_system->volume() * gain;
#else
    Q_UNUSED(this);
#endif
}

void MediaSession::updateSource()
{
    if (m_remoteActive || m_btActive) {
        emit sourceChanged();
        return;
    }
    const QString source = QStringLiteral("本机");
    if (source == m_source) {
        emit sourceChanged();
        return;
    }
    m_source = source;
    emit sourceChanged();
}

void MediaSession::poll()
{
    if (!m_playing || m_tracks.isEmpty() || m_advancing)
        return;
#ifdef Q_OS_WIN
    const int ms = mciStatus(L"status iviMusic position").toInt();
    const int seconds = ms / 1000;
    if (seconds != m_position) {
        m_position = seconds;
        emit positionChanged();
    }
    const int length = mciStatus(L"status iviMusic length").toInt();
    if (mciStatus(L"status iviMusic mode") == QStringLiteral("stopped") && length > 0 && ms + 400 >= length) {
        m_advancing = true;
        if (m_playMode == Single) {
            seek(0);
            play();
        } else {
            advance(1, true);
        }
        m_advancing = false;
    }
#elif defined(IVI_HAVE_ALSA)
    g_alsaMusic.writeMs(500);
    m_softMs += 500;
    const int seconds = m_softMs / 1000;
    if (seconds != m_position) {
        m_position = seconds;
        emit positionChanged();
    }
    if (g_alsaMusic.ended() || m_position >= duration()) {
        m_advancing = true;
        if (m_playMode == Single) {
            seek(0);
            play();
        } else {
            advance(1, true);
        }
        m_advancing = false;
    }
#else
    m_softMs += 500;
    const int seconds = m_softMs / 1000;
    if (seconds != m_position) {
        m_position = seconds;
        emit positionChanged();
    }
    if (m_position >= duration()) {
        m_advancing = true;
        if (m_playMode == Single) {
            seek(0);
            play();
        } else {
            advance(1, true);
        }
        m_advancing = false;
    }
#endif
}

void MediaSession::select(int index, bool start)
{
    if (index < 0 || index >= m_tracks.size())
        return;
    if (m_advancing && index == m_index && start)
        return;
    m_index = index;
    m_position = 0;
    m_softMs = 0;
    emit trackChanged();
    emit positionChanged();
    openCurrent();
    savePrefs();
    if (start)
        play();
}

void MediaSession::ensureQueue()
{
    if (!m_queue.isEmpty() || m_tracks.isEmpty())
        return;
    for (int i = 0; i < m_tracks.size(); ++i)
        m_queue.push_back(i);
    rebuildShuffle();
    emit queueChanged();
}

void MediaSession::rebuildShuffle()
{
    m_shuffle = m_queue;
    if (m_shuffle.size() <= 1)
        return;
    auto *rng = QRandomGenerator::global();
    for (int i = m_shuffle.size() - 1; i > 0; --i) {
        const int j = rng->bounded(i + 1);
        qSwap(m_shuffle[i], m_shuffle[j]);
    }
    if (m_shuffle.contains(m_index) && m_shuffle.first() != m_index) {
        const int pos = m_shuffle.indexOf(m_index);
        qSwap(m_shuffle[0], m_shuffle[pos]);
    }
}

int MediaSession::queuePosOf(int trackIndex) const
{
    return m_queue.indexOf(trackIndex);
}

void MediaSession::advance(int delta, bool fromEnd)
{
    Q_UNUSED(fromEnd);
    ensureQueue();
    if (m_queue.isEmpty())
        return;

    const QVector<int> &order = (m_playMode == Shuffle) ? m_shuffle : m_queue;
    int pos = order.indexOf(m_index);
    if (pos < 0)
        pos = 0;
    int nextPos = pos + delta;
    if (nextPos >= order.size()) {
        if (m_playMode == Shuffle)
            rebuildShuffle();
        nextPos = 0;
    } else if (nextPos < 0) {
        nextPos = order.size() - 1;
    }
    select(order.at(nextPos), true);
}

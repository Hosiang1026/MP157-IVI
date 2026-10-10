#include "RadioSession.hpp"

#include "AudioFocus.hpp"
#include "MediaSession.hpp"

#include <QSettings>
#include <QUrl>
#include <QVariantMap>

namespace {

QVariantMap station(const QString &name, const QString &url, const QString &genre)
{
    QVariantMap map;
    map.insert(QStringLiteral("name"), name);
    map.insert(QStringLiteral("url"), url);
    map.insert(QStringLiteral("genre"), genre);
    return map;
}

} // namespace

RadioSession::RadioSession(AudioFocus *audio, MediaSession *media, QObject *parent)
    : QObject(parent)
    , m_audio(audio)
    , m_media(media)
    , m_status(QStringLiteral("未播放"))
{
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, [this] {
        if (!m_wantPlay || m_index < 0)
            return;
        setReconnecting(true);
        setStatus(QStringLiteral("重连中…"));
        startPlay(m_index, false);
    });

    loadFavorites();
    loadStations();
    connect(&m_player, &FfmpegUrlPlayer::playingChanged, this, &RadioSession::playingChanged);
    connect(&m_player, &FfmpegUrlPlayer::errorOccurred, this, [this](const QString &msg) {
        setDetail(msg);
        if (m_wantPlay) {
            setStatus(QStringLiteral("连接中断"));
            scheduleReconnect();
            return;
        }
        setStatus(QStringLiteral("播放失败"));
        setReconnecting(false);
        if (m_audio)
            m_audio->release(QStringLiteral("radio"));
    });
    connect(&m_player, &FfmpegUrlPlayer::finished, this, [this] {
        if (m_wantPlay) {
            setStatus(QStringLiteral("连接中断"));
            scheduleReconnect();
            return;
        }
        setStatus(QStringLiteral("已停止"));
        setDetail({});
        setReconnecting(false);
        if (m_audio)
            m_audio->release(QStringLiteral("radio"));
        emit playingChanged();
    });
    if (m_audio) {
        connect(m_audio, &AudioFocus::ownerChanged, this, [this] {
            if (m_player.playing() && m_audio->owner() != QLatin1String("radio"))
                stop();
        });
    }
}

RadioSession::~RadioSession()
{
    m_wantPlay = false;
    m_retryTimer.stop();
    stop();
}

void RadioSession::loadStations()
{
    m_allStations = {
        station(QStringLiteral("中国之声"), QStringLiteral("https://lhttp.qingting.fm/live/386/64k.mp3"),
                QStringLiteral("新闻")),
        station(QStringLiteral("经济之声"), QStringLiteral("https://lhttp.qingting.fm/live/387/64k.mp3"),
                QStringLiteral("财经")),
        station(QStringLiteral("音乐之声"), QStringLiteral("https://lhttp.qingting.fm/live/389/64k.mp3"),
                QStringLiteral("音乐")),
        station(QStringLiteral("文艺之声"), QStringLiteral("https://lhttp.qingting.fm/live/401/64k.mp3"),
                QStringLiteral("文艺")),
        station(QStringLiteral("Hit FM"), QStringLiteral("https://lhttp.qingting.fm/live/4804/64k.mp3"),
                QStringLiteral("流行")),
        station(QStringLiteral("环球资讯"), QStringLiteral("https://lhttp.qingting.fm/live/1005/64k.mp3"),
                QStringLiteral("资讯")),
        station(QStringLiteral("古典音乐"), QStringLiteral("https://lhttp.qingting.fm/live/388/64k.mp3"),
                QStringLiteral("古典")),
        station(QStringLiteral("BBC World"),
                QStringLiteral("http://stream.live.vc.bbcmedia.co.uk/bbc_world_service"),
                QStringLiteral("国际")),
        station(QStringLiteral("NPR News"), QStringLiteral("https://npr-ice.streamguys1.com/live.mp3"),
                QStringLiteral("国际")),
        station(QStringLiteral("SomaFM Groove"), QStringLiteral("https://ice1.somafm.com/groovesalad-128-mp3"),
                QStringLiteral("电子")),
        station(QStringLiteral("SomaFM Drone"), QStringLiteral("https://ice1.somafm.com/dronezone-128-mp3"),
                QStringLiteral("氛围")),
        station(QStringLiteral("Radio Paradise"), QStringLiteral("https://stream.radioparadise.com/aac-128"),
                QStringLiteral("流行")),
        station(QStringLiteral("FIP"), QStringLiteral("https://icecast.radiofrance.fr/fip-midfi.mp3"),
                QStringLiteral("音乐")),
        station(QStringLiteral("Jazz24"), QStringLiteral("https://live.wostreaming.net/direct/ppm-jazz24mp3-ibc1"),
                QStringLiteral("爵士")),
    };
    rebuildStations();
}

void RadioSession::loadFavorites()
{
    m_favorites = QSettings().value(QStringLiteral("radio/favorites")).toStringList();
}

void RadioSession::saveFavorites()
{
    QSettings().setValue(QStringLiteral("radio/favorites"), m_favorites);
}

void RadioSession::rebuildStations()
{
    m_stations.clear();
    for (const QVariant &item : m_allStations) {
        const QVariantMap map = item.toMap();
        const QString url = map.value(QStringLiteral("url")).toString();
        if (m_filterMode == 1 && !m_favorites.contains(url))
            continue;
        QVariantMap row = map;
        row.insert(QStringLiteral("favorite"), m_favorites.contains(url));
        m_stations.push_back(row);
    }
    emit stationsChanged();
}

void RadioSession::setFilterMode(int mode)
{
    const int m = mode == 1 ? 1 : 0;
    if (m_filterMode == m)
        return;
    const QString keepUrl = m_currentUrl;
    m_filterMode = m;
    emit filterModeChanged();
    rebuildStations();
    m_index = -1;
    for (int i = 0; i < m_stations.size(); ++i) {
        if (m_stations.at(i).toMap().value(QStringLiteral("url")).toString() == keepUrl) {
            m_index = i;
            break;
        }
    }
    emit stationChanged();
}

void RadioSession::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void RadioSession::setDetail(const QString &detail)
{
    if (m_detail == detail)
        return;
    m_detail = detail;
    emit detailChanged();
}

void RadioSession::setReconnecting(bool on)
{
    if (m_reconnecting == on)
        return;
    m_reconnecting = on;
    emit reconnectingChanged();
}

QVariantMap RadioSession::stationAt(int index) const
{
    if (index < 0 || index >= m_stations.size())
        return {};
    return m_stations.at(index).toMap();
}

bool RadioSession::isFavorite(int index) const
{
    const QString url = stationAt(index).value(QStringLiteral("url")).toString();
    return !url.isEmpty() && m_favorites.contains(url);
}

void RadioSession::toggleFavorite(int index)
{
    const QString url = stationAt(index).value(QStringLiteral("url")).toString();
    if (url.isEmpty())
        return;
    if (m_favorites.contains(url))
        m_favorites.removeAll(url);
    else
        m_favorites.push_back(url);
    saveFavorites();
    rebuildStations();
}

void RadioSession::scheduleReconnect()
{
    if (!m_wantPlay)
        return;
    if (m_retryCount >= 8) {
        setStatus(QStringLiteral("重连失败"));
        setReconnecting(false);
        m_wantPlay = false;
        if (m_audio)
            m_audio->release(QStringLiteral("radio"));
        return;
    }
    ++m_retryCount;
    setReconnecting(true);
    const int delay = qMin(15000, 1500 * m_retryCount);
    setDetail(QStringLiteral("将在 %1 秒后重连").arg((delay + 999) / 1000));
    m_retryTimer.start(delay);
}

void RadioSession::startPlay(int index, bool fromUser)
{
    const QVariantMap item = stationAt(index);
    const QString url = item.value(QStringLiteral("url")).toString();
    const QString name = item.value(QStringLiteral("name")).toString();
    if (url.isEmpty())
        return;

    if (fromUser) {
        m_retryCount = 0;
        m_retryTimer.stop();
        setReconnecting(false);
    }

    if (m_media && m_media->playing())
        m_media->pause();
    if (m_audio)
        m_audio->request(QStringLiteral("radio"), m_audio->radioPriority());

    m_wantPlay = true;
    m_index = index;
    m_stationName = name;
    m_currentUrl = url;
    emit stationChanged();
    setStatus(m_reconnecting ? QStringLiteral("重连中…") : QStringLiteral("连接中…"));
    setDetail(url);
    m_player.play(QUrl(url));
    if (m_player.playing())
        setStatus(QStringLiteral("播放中"));
}

void RadioSession::playIndex(int index)
{
    if (index < 0 || index >= m_stations.size())
        return;
    startPlay(index, true);
    if (m_player.playing())
        setStatus(QStringLiteral("播放中"));
}

void RadioSession::stop()
{
    m_wantPlay = false;
    m_retryCount = 0;
    m_retryTimer.stop();
    setReconnecting(false);
    m_player.stop();
    if (m_audio)
        m_audio->release(QStringLiteral("radio"));
    setStatus(QStringLiteral("已停止"));
    setDetail({});
    emit playingChanged();
}

void RadioSession::toggle()
{
    if (m_player.playing() || m_wantPlay) {
        stop();
        return;
    }
    playIndex(m_index >= 0 ? m_index : 0);
}

void RadioSession::next()
{
    if (m_stations.isEmpty())
        return;
    const int i = m_index < 0 ? 0 : (m_index + 1) % m_stations.size();
    playIndex(i);
}

void RadioSession::previous()
{
    if (m_stations.isEmpty())
        return;
    const int n = m_stations.size();
    const int i = m_index < 0 ? 0 : (m_index - 1 + n) % n;
    playIndex(i);
}

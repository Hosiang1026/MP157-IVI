#include "RadioSession.hpp"

#include "AudioFocus.hpp"
#include "MediaSession.hpp"

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

}

RadioSession::RadioSession(AudioFocus *audio, MediaSession *media, QObject *parent)
    : QObject(parent)
    , m_audio(audio)
    , m_media(media)
    , m_status(QStringLiteral("未播放"))
{
    loadStations();
    connect(&m_player, &FfmpegUrlPlayer::playingChanged, this, &RadioSession::playingChanged);
    connect(&m_player, &FfmpegUrlPlayer::errorOccurred, this, [this](const QString &msg) {
        setStatus(QStringLiteral("播放失败"));
        setDetail(msg);
        if (m_audio)
            m_audio->release(QStringLiteral("radio"));
    });
    connect(&m_player, &FfmpegUrlPlayer::finished, this, [this] {
        setStatus(QStringLiteral("已停止"));
        setDetail({});
        if (m_audio)
            m_audio->release(QStringLiteral("radio"));
        emit playingChanged();
    });
}

RadioSession::~RadioSession()
{
    stop();
}

void RadioSession::loadStations()
{
    m_stations = {
        station(QStringLiteral("中国之声"), QStringLiteral("https://lhttp.qingting.fm/live/386/64k.mp3"),
                QStringLiteral("新闻")),
        station(QStringLiteral("经济之声"), QStringLiteral("https://lhttp.qingting.fm/live/387/64k.mp3"),
                QStringLiteral("财经")),
        station(QStringLiteral("音乐之声"), QStringLiteral("https://lhttp.qingting.fm/live/389/64k.mp3"),
                QStringLiteral("音乐")),
        station(QStringLiteral("Hit FM"), QStringLiteral("https://lhttp.qingting.fm/live/4804/64k.mp3"),
                QStringLiteral("流行")),
        station(QStringLiteral("环球资讯"), QStringLiteral("https://lhttp.qingting.fm/live/1005/64k.mp3"),
                QStringLiteral("资讯")),
        station(QStringLiteral("BBC World"),
                QStringLiteral("http://stream.live.vc.bbcmedia.co.uk/bbc_world_service"),
                QStringLiteral("国际")),
        station(QStringLiteral("NPR News"), QStringLiteral("https://npr-ice.streamguys1.com/live.mp3"),
                QStringLiteral("国际")),
        station(QStringLiteral("SomaFM Groove"), QStringLiteral("https://ice1.somafm.com/groovesalad-128-mp3"),
                QStringLiteral("电子")),
    };
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

void RadioSession::playIndex(int index)
{
    if (index < 0 || index >= m_stations.size())
        return;
    const QVariantMap item = m_stations.at(index).toMap();
    const QString url = item.value(QStringLiteral("url")).toString();
    const QString name = item.value(QStringLiteral("name")).toString();
    if (url.isEmpty())
        return;

    if (m_media && m_media->playing())
        m_media->pause();
    if (m_audio)
        m_audio->request(QStringLiteral("radio"), m_audio->mediaPriority());

    m_index = index;
    m_stationName = name;
    emit stationChanged();
    setStatus(QStringLiteral("连接中…"));
    setDetail(url);
    m_player.play(QUrl(url));
    setStatus(QStringLiteral("播放中"));
}

void RadioSession::stop()
{
    m_player.stop();
    if (m_audio)
        m_audio->release(QStringLiteral("radio"));
    setStatus(QStringLiteral("已停止"));
    setDetail({});
    emit playingChanged();
}

void RadioSession::toggle()
{
    if (m_player.playing()) {
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

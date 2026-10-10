#include "PodcastSession.hpp"

#include "AudioFocus.hpp"
#include "MediaSession.hpp"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QVariantMap>
#include <QXmlStreamReader>

namespace {

QVariantMap feedItem(const QString &name, const QString &url)
{
    QVariantMap map;
    map.insert(QStringLiteral("name"), name);
    map.insert(QStringLiteral("url"), url);
    return map;
}

QString textOf(QXmlStreamReader &xml)
{
    return xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
}

} // namespace

PodcastSession::PodcastSession(AudioFocus *audio, MediaSession *media, QObject *parent)
    : QObject(parent)
    , m_audio(audio)
    , m_media(media)
    , m_status(QStringLiteral("未播放"))
{
    loadFeeds();
    connect(&m_player, &FfmpegUrlPlayer::playingChanged, this, &PodcastSession::playingChanged);
    connect(&m_player, &FfmpegUrlPlayer::errorOccurred, this, [this](const QString &msg) {
        setStatus(QStringLiteral("播放失败"));
        setDetail(msg);
        if (m_audio)
            m_audio->release(QStringLiteral("podcast"));
    });
    connect(&m_player, &FfmpegUrlPlayer::finished, this, [this] {
        setStatus(QStringLiteral("已停止"));
        setDetail({});
        if (m_audio)
            m_audio->release(QStringLiteral("podcast"));
        emit playingChanged();
    });
    if (m_audio) {
        connect(m_audio, &AudioFocus::ownerChanged, this, [this] {
            if (m_player.playing() && m_audio->owner() != QLatin1String("podcast"))
                stop();
        });
    }
    applyLibraryFeeds(true);
}

PodcastSession::~PodcastSession()
{
    stop();
}

void PodcastSession::loadFeeds()
{
    QSettings s;
    const QVariantList podcastSaved = s.value(QStringLiteral("podcast/feeds")).toList();
    if (!podcastSaved.isEmpty()) {
        m_podcastFeeds = podcastSaved;
    } else {
        m_podcastFeeds = {
            feedItem(QStringLiteral("BBC Global News"),
                     QStringLiteral("https://podcasts.files.bbci.co.uk/p02nq0gn.rss")),
            feedItem(QStringLiteral("NPR News Now"),
                     QStringLiteral("https://feeds.npr.org/500005/podcast.xml")),
            feedItem(QStringLiteral("TED Talks Daily"),
                     QStringLiteral("https://feeds.feedburner.com/TEDTalks_audio")),
        };
        s.setValue(QStringLiteral("podcast/feeds"), m_podcastFeeds);
    }

    const QVariantList bookSaved = s.value(QStringLiteral("podcast/bookFeeds")).toList();
    if (!bookSaved.isEmpty()) {
        m_bookFeeds = bookSaved;
    } else {
        m_bookFeeds = {
            feedItem(QStringLiteral("Alice's Adventures in Wonderland"),
                     QStringLiteral("https://librivox.org/rss/27")),
            feedItem(QStringLiteral("Pride and Prejudice"),
                     QStringLiteral("https://librivox.org/rss/104")),
            feedItem(QStringLiteral("The Adventures of Sherlock Holmes"),
                     QStringLiteral("https://librivox.org/rss/55")),
        };
        s.setValue(QStringLiteral("podcast/bookFeeds"), m_bookFeeds);
    }
}

void PodcastSession::saveFeeds()
{
    if (m_library == QLatin1String("book")) {
        m_bookFeeds = m_feeds;
        QSettings().setValue(QStringLiteral("podcast/bookFeeds"), m_bookFeeds);
    } else {
        m_podcastFeeds = m_feeds;
        QSettings().setValue(QStringLiteral("podcast/feeds"), m_podcastFeeds);
    }
}

void PodcastSession::applyLibraryFeeds(bool autoSelect)
{
    m_feeds = (m_library == QLatin1String("book")) ? m_bookFeeds : m_podcastFeeds;
    m_feedIndex = -1;
    m_index = -1;
    m_feedTitle.clear();
    m_episodes.clear();
    emit feedsChanged();
    emit episodesChanged();
    emit feedChanged();
    emit episodeChanged();
    if (autoSelect && !m_feeds.isEmpty())
        selectFeed(0);
}

void PodcastSession::setLibrary(const QString &library)
{
    const QString lib = (library == QLatin1String("book")) ? QStringLiteral("book")
                                                           : QStringLiteral("podcast");
    if (m_library == lib)
        return;
    saveFeeds();
    m_library = lib;
    emit libraryChanged();
    applyLibraryFeeds(true);
}

void PodcastSession::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void PodcastSession::setDetail(const QString &detail)
{
    if (m_detail == detail)
        return;
    m_detail = detail;
    emit detailChanged();
}

void PodcastSession::setLoading(bool on)
{
    if (m_loading == on)
        return;
    m_loading = on;
    emit loadingChanged();
}

void PodcastSession::selectFeed(int index)
{
    if (index < 0 || index >= m_feeds.size())
        return;
    m_feedIndex = index;
    const QVariantMap item = m_feeds.at(index).toMap();
    m_feedTitle = item.value(QStringLiteral("name")).toString();
    emit feedChanged();
    fetchFeed(item.value(QStringLiteral("url")).toString());
}

void PodcastSession::refresh()
{
    if (m_feedIndex < 0 || m_feedIndex >= m_feeds.size())
        return;
    selectFeed(m_feedIndex);
}

void PodcastSession::addFeed(const QString &url, const QString &name)
{
    const QString u = url.trimmed();
    if (u.isEmpty())
        return;
    for (const QVariant &item : m_feeds) {
        if (item.toMap().value(QStringLiteral("url")).toString() == u)
            return;
    }
    m_feeds.push_back(feedItem(name.trimmed().isEmpty() ? u : name.trimmed(), u));
    saveFeeds();
    emit feedsChanged();
    selectFeed(m_feeds.size() - 1);
}

void PodcastSession::removeFeed(int index)
{
    if (index < 0 || index >= m_feeds.size())
        return;
    m_feeds.removeAt(index);
    saveFeeds();
    emit feedsChanged();
    if (m_feeds.isEmpty()) {
        m_feedIndex = -1;
        m_feedTitle.clear();
        m_episodes.clear();
        emit feedChanged();
        emit episodesChanged();
        return;
    }
    selectFeed(qMin(index, m_feeds.size() - 1));
}

void PodcastSession::fetchFeed(const QString &url)
{
    if (url.isEmpty())
        return;
    setLoading(true);
    setDetail(url);
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MP157-IVI-Podcast/1.0"));
    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        setLoading(false);
        if (reply->error() != QNetworkReply::NoError) {
            setDetail(reply->errorString());
            m_episodes.clear();
            emit episodesChanged();
            return;
        }
        parseRss(reply->readAll());
    });
}

void PodcastSession::parseRss(const QByteArray &data)
{
    m_episodes.clear();
    QXmlStreamReader xml(data);
    QString channelTitle;
    QString itemTitle;
    QString itemUrl;
    QString itemDate;
    bool inItem = false;

    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            const QStringView name = xml.name();
            if (name == QLatin1String("item") || name == QLatin1String("entry")) {
                inItem = true;
                itemTitle.clear();
                itemUrl.clear();
                itemDate.clear();
            } else if (!inItem && name == QLatin1String("title")) {
                channelTitle = textOf(xml);
            } else if (inItem && name == QLatin1String("title")) {
                itemTitle = textOf(xml);
            } else if (inItem && (name == QLatin1String("pubDate") || name == QLatin1String("published")
                                  || name == QLatin1String("updated"))) {
                itemDate = textOf(xml);
            } else if (inItem && name == QLatin1String("enclosure")) {
                const QString href = xml.attributes().value(QStringLiteral("url")).toString();
                const QString type = xml.attributes().value(QStringLiteral("type")).toString();
                if (!href.isEmpty() && (type.isEmpty() || type.startsWith(QLatin1String("audio"))
                                        || href.contains(QLatin1String(".mp3"))
                                        || href.contains(QLatin1String(".m4a"))
                                        || href.contains(QLatin1String(".aac")))) {
                    itemUrl = href;
                }
            } else if (inItem && name == QLatin1String("link")) {
                const QString href = xml.attributes().value(QStringLiteral("href")).toString();
                const QString rel = xml.attributes().value(QStringLiteral("rel")).toString();
                const QString type = xml.attributes().value(QStringLiteral("type")).toString();
                if (!href.isEmpty() && (rel == QLatin1String("enclosure")
                                        || type.startsWith(QLatin1String("audio"))
                                        || href.contains(QLatin1String(".mp3")))) {
                    itemUrl = href;
                }
            }
        } else if (token == QXmlStreamReader::EndElement) {
            const QStringView name = xml.name();
            if (name == QLatin1String("item") || name == QLatin1String("entry")) {
                inItem = false;
                if (!itemUrl.isEmpty()) {
                    QVariantMap ep;
                    ep.insert(QStringLiteral("title"),
                              itemTitle.isEmpty() ? QStringLiteral("未命名") : itemTitle);
                    ep.insert(QStringLiteral("url"), itemUrl);
                    ep.insert(QStringLiteral("date"), itemDate);
                    m_episodes.push_back(ep);
                    if (m_episodes.size() >= 120)
                        break;
                }
            }
        }
    }

    if (!channelTitle.isEmpty() && m_feedIndex >= 0 && m_feedIndex < m_feeds.size()) {
        QVariantMap feed = m_feeds[m_feedIndex].toMap();
        if (feed.value(QStringLiteral("name")).toString() == feed.value(QStringLiteral("url")).toString()
            || feed.value(QStringLiteral("name")).toString().startsWith(QLatin1String("http"))) {
            feed.insert(QStringLiteral("name"), channelTitle);
            m_feeds[m_feedIndex] = feed;
            m_feedTitle = channelTitle;
            saveFeeds();
            emit feedsChanged();
            emit feedChanged();
        }
    }

    emit episodesChanged();
    setDetail(m_episodes.isEmpty() ? QStringLiteral("无音频条目")
                                   : QStringLiteral("%1 集").arg(m_episodes.size()));
}

void PodcastSession::playIndex(int index)
{
    if (index < 0 || index >= m_episodes.size())
        return;
    const QVariantMap item = m_episodes.at(index).toMap();
    const QString url = item.value(QStringLiteral("url")).toString();
    const QString title = item.value(QStringLiteral("title")).toString();
    if (url.isEmpty())
        return;

    if (m_media && m_media->playing())
        m_media->pause();
    if (m_audio)
        m_audio->request(QStringLiteral("podcast"), m_audio->radioPriority());

    m_index = index;
    m_episodeTitle = title;
    emit episodeChanged();
    setStatus(QStringLiteral("连接中…"));
    setDetail(url);
    m_player.play(QUrl(url));
    setStatus(QStringLiteral("播放中"));
}

void PodcastSession::stop()
{
    m_player.stop();
    if (m_audio)
        m_audio->release(QStringLiteral("podcast"));
    setStatus(QStringLiteral("已停止"));
    setDetail({});
    emit playingChanged();
}

void PodcastSession::toggle()
{
    if (m_player.playing()) {
        stop();
        return;
    }
    playIndex(m_index >= 0 ? m_index : 0);
}

void PodcastSession::next()
{
    if (m_episodes.isEmpty())
        return;
    const int i = m_index < 0 ? 0 : (m_index + 1) % m_episodes.size();
    playIndex(i);
}

void PodcastSession::previous()
{
    if (m_episodes.isEmpty())
        return;
    const int n = m_episodes.size();
    const int i = m_index < 0 ? 0 : (m_index - 1 + n) % n;
    playIndex(i);
}

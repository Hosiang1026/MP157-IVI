#pragma once

#include "FfmpegUrlPlayer.hpp"

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QVariantList>

class AudioFocus;
class MediaSession;

class PodcastSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY detailChanged)
    Q_PROPERTY(QString episodeTitle READ episodeTitle NOTIFY episodeChanged)
    Q_PROPERTY(QString feedTitle READ feedTitle NOTIFY feedChanged)
    Q_PROPERTY(QString library READ library WRITE setLibrary NOTIFY libraryChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY episodeChanged)
    Q_PROPERTY(int feedIndex READ feedIndex NOTIFY feedChanged)
    Q_PROPERTY(QVariantList feeds READ feeds NOTIFY feedsChanged)
    Q_PROPERTY(QVariantList episodes READ episodes NOTIFY episodesChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
public:
    PodcastSession(AudioFocus *audio, MediaSession *media, QObject *parent = nullptr);
    ~PodcastSession() override;

    bool playing() const { return m_player.playing(); }
    QString status() const { return m_status; }
    QString detail() const { return m_detail; }
    QString episodeTitle() const { return m_episodeTitle; }
    QString feedTitle() const { return m_feedTitle; }
    QString library() const { return m_library; }
    void setLibrary(const QString &library);
    int currentIndex() const { return m_index; }
    int feedIndex() const { return m_feedIndex; }
    QVariantList feeds() const { return m_feeds; }
    QVariantList episodes() const { return m_episodes; }
    bool loading() const { return m_loading; }

    Q_INVOKABLE void selectFeed(int index);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void addFeed(const QString &url, const QString &name = QString());
    Q_INVOKABLE void removeFeed(int index);
    Q_INVOKABLE void playIndex(int index);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();

signals:
    void playingChanged();
    void statusChanged();
    void detailChanged();
    void episodeChanged();
    void feedChanged();
    void feedsChanged();
    void episodesChanged();
    void loadingChanged();
    void libraryChanged();

private:
    void setStatus(const QString &status);
    void setDetail(const QString &detail);
    void setLoading(bool on);
    void loadFeeds();
    void saveFeeds();
    void applyLibraryFeeds(bool autoSelect);
    void fetchFeed(const QString &url);
    void parseRss(const QByteArray &data);

    AudioFocus *m_audio = nullptr;
    MediaSession *m_media = nullptr;
    FfmpegUrlPlayer m_player;
    QNetworkAccessManager m_net;
    QVariantList m_podcastFeeds;
    QVariantList m_bookFeeds;
    QVariantList m_feeds;
    QVariantList m_episodes;
    QString m_library = QStringLiteral("podcast");
    QString m_status;
    QString m_detail;
    QString m_episodeTitle;
    QString m_feedTitle;
    int m_index = -1;
    int m_feedIndex = 0;
    bool m_loading = false;
};

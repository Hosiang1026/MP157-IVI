#pragma once

#include "FfmpegUrlPlayer.hpp"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantList>

#include <functional>

class AudioFocus;
class MediaSession;

class StreamSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY detailChanged)
    Q_PROPERTY(QString backend READ backend WRITE setBackend NOTIFY configChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY configChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY configChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY configChanged)
    Q_PROPERTY(QString title READ title NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(QString album READ album NOTIFY trackChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY trackChanged)
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList albums READ albums NOTIFY albumsChanged)
public:
    StreamSession(AudioFocus *audio, MediaSession *media, QObject *parent = nullptr);
    ~StreamSession() override;

    bool playing() const { return m_player.playing(); }
    bool loggedIn() const { return m_loggedIn; }
    bool loading() const { return m_loading; }
    QString status() const { return m_status; }
    QString detail() const { return m_detail; }
    QString backend() const { return m_backend; }
    void setBackend(const QString &v);
    QString serverUrl() const { return m_serverUrl; }
    void setServerUrl(const QString &v);
    QString username() const { return m_username; }
    void setUsername(const QString &v);
    QString password() const { return m_password; }
    void setPassword(const QString &v);
    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    QString album() const { return m_album; }
    int currentIndex() const { return m_index; }
    QVariantList tracks() const { return m_tracks; }
    QVariantList albums() const { return m_albums; }

    Q_INVOKABLE void saveConfig();
    Q_INVOKABLE void login();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void loadAlbums();
    Q_INVOKABLE void openAlbum(const QString &id);
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void playIndex(int index);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();

signals:
    void playingChanged();
    void loggedInChanged();
    void loadingChanged();
    void statusChanged();
    void detailChanged();
    void configChanged();
    void trackChanged();
    void tracksChanged();
    void albumsChanged();

private:
    void setStatus(const QString &status);
    void setDetail(const QString &detail);
    void setLoading(bool on);
    void setLoggedIn(bool on);
    void loadConfig();
    QString normalizedBase() const;
    QUrl subsonicUrl(const QString &action, const QUrlQuery &extra = {}) const;
    void loginSubsonic();
    void loginJellyfin();
    void loadAlbumsSubsonic();
    void loadAlbumsJellyfin();
    void openAlbumSubsonic(const QString &id);
    void openAlbumJellyfin(const QString &id);
    void searchSubsonic(const QString &query);
    void searchJellyfin(const QString &query);
    QString streamUrlFor(const QVariantMap &track) const;
    void getJson(const QUrl &url, const std::function<void(const QJsonObject &)> &ok);
    void getJsonAuth(const QUrl &url, const std::function<void(const QJsonObject &)> &ok);

    AudioFocus *m_audio = nullptr;
    MediaSession *m_media = nullptr;
    FfmpegUrlPlayer m_player;
    QNetworkAccessManager m_net;
    QString m_backend = QStringLiteral("subsonic");
    QString m_serverUrl;
    QString m_username;
    QString m_password;
    QString m_token;
    QString m_salt;
    QString m_userId;
    QString m_deviceId = QStringLiteral("mp157-ivi");
    QString m_status;
    QString m_detail;
    QString m_title;
    QString m_artist;
    QString m_album;
    QVariantList m_tracks;
    QVariantList m_albums;
    int m_index = -1;
    bool m_loggedIn = false;
    bool m_loading = false;
};

#pragma once

#include "FfmpegUrlPlayer.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

class AudioFocus;
class MediaSession;

class RadioSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY detailChanged)
    Q_PROPERTY(QString stationName READ stationName NOTIFY stationChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY stationChanged)
    Q_PROPERTY(QVariantList stations READ stations NOTIFY stationsChanged)
    Q_PROPERTY(int filterMode READ filterMode WRITE setFilterMode NOTIFY filterModeChanged)
    Q_PROPERTY(bool reconnecting READ reconnecting NOTIFY reconnectingChanged)
public:
    RadioSession(AudioFocus *audio, MediaSession *media, QObject *parent = nullptr);
    ~RadioSession() override;

    bool playing() const { return m_player.playing(); }
    QString status() const { return m_status; }
    QString detail() const { return m_detail; }
    QString stationName() const { return m_stationName; }
    int currentIndex() const { return m_index; }
    QVariantList stations() const { return m_stations; }
    int filterMode() const { return m_filterMode; }
    void setFilterMode(int mode);
    bool reconnecting() const { return m_reconnecting; }

    Q_INVOKABLE void playIndex(int index);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void toggleFavorite(int index);
    Q_INVOKABLE bool isFavorite(int index) const;

signals:
    void playingChanged();
    void statusChanged();
    void detailChanged();
    void stationChanged();
    void stationsChanged();
    void filterModeChanged();
    void reconnectingChanged();

private:
    void setStatus(const QString &status);
    void setDetail(const QString &detail);
    void setReconnecting(bool on);
    void loadStations();
    void rebuildStations();
    void loadFavorites();
    void saveFavorites();
    void startPlay(int index, bool fromUser);
    void scheduleReconnect();
    QVariantMap stationAt(int index) const;

    AudioFocus *m_audio = nullptr;
    MediaSession *m_media = nullptr;
    FfmpegUrlPlayer m_player;
    QVariantList m_allStations;
    QVariantList m_stations;
    QStringList m_favorites;
    QString m_status;
    QString m_detail;
    QString m_stationName;
    QString m_currentUrl;
    int m_index = -1;
    int m_filterMode = 0;
    bool m_wantPlay = false;
    bool m_reconnecting = false;
    int m_retryCount = 0;
    QTimer m_retryTimer;
};

#pragma once

#include "FfmpegUrlPlayer.hpp"

#include <QObject>
#include <QString>
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
    Q_PROPERTY(QVariantList stations READ stations CONSTANT)
public:
    RadioSession(AudioFocus *audio, MediaSession *media, QObject *parent = nullptr);
    ~RadioSession() override;

    bool playing() const { return m_player.playing(); }
    QString status() const { return m_status; }
    QString detail() const { return m_detail; }
    QString stationName() const { return m_stationName; }
    int currentIndex() const { return m_index; }
    QVariantList stations() const { return m_stations; }

    Q_INVOKABLE void playIndex(int index);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();

signals:
    void playingChanged();
    void statusChanged();
    void detailChanged();
    void stationChanged();

private:
    void setStatus(const QString &status);
    void setDetail(const QString &detail);
    void loadStations();

    AudioFocus *m_audio = nullptr;
    MediaSession *m_media = nullptr;
    FfmpegUrlPlayer m_player;
    QVariantList m_stations;
    QString m_status;
    QString m_detail;
    QString m_stationName;
    int m_index = -1;
};

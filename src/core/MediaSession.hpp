#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVector>

class AudioFocus;
class SystemState;

class MediaSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(QString source READ source NOTIFY sourceChanged)
    Q_PROPERTY(QString title READ title NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(int trackIndex READ trackIndex NOTIFY trackChanged)
    Q_PROPERTY(int position READ position NOTIFY positionChanged)
    Q_PROPERTY(int duration READ duration NOTIFY trackChanged)
    Q_PROPERTY(QVariantList tracks READ tracks CONSTANT)
public:
    MediaSession(SystemState *system, AudioFocus *audio, QObject *parent = nullptr);
    ~MediaSession() override;

    bool playing() const;
    QString source() const;
    QString title() const;
    QString artist() const;
    int trackIndex() const;
    int position() const;
    int duration() const;
    QVariantList tracks() const;

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void playIndex(int index);
    Q_INVOKABLE void seek(int seconds);

signals:
    void playingChanged();
    void sourceChanged();
    void trackChanged();
    void positionChanged();

private:
    struct Track {
        QString title;
        QString artist;
        QString file;
        int duration = 1;
    };

    void loadTracks();
    void openCurrent();
    void applyVolume();
    void updateSource();
    void poll();
    void select(int index, bool start);

    SystemState *m_system = nullptr;
    AudioFocus *m_audio = nullptr;
    QVector<Track> m_tracks;
    int m_index = 0;
    int m_position = 0;
    bool m_playing = false;
    bool m_advancing = false;
    QString m_source;
    QTimer m_timer;
};

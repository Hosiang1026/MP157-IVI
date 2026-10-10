#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVector>

class AudioFocus;
class SystemState;

class MediaSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(QString source READ source NOTIFY sourceChanged)
    Q_PROPERTY(bool bluetoothMode READ bluetoothMode NOTIFY sourceChanged)
    Q_PROPERTY(QString title READ title NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(QString coverColor READ coverColor NOTIFY trackChanged)
    Q_PROPERTY(int trackIndex READ trackIndex NOTIFY trackChanged)
    Q_PROPERTY(int position READ position NOTIFY positionChanged)
    Q_PROPERTY(int duration READ duration NOTIFY trackChanged)
    Q_PROPERTY(int playMode READ playMode NOTIFY playModeChanged)
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList queue READ queue NOTIFY queueChanged)
    Q_PROPERTY(QStringList lyrics READ lyrics NOTIFY trackChanged)
public:
    enum PlayMode {
        Loop = 0,
        Single = 1,
        Shuffle = 2
    };
    Q_ENUM(PlayMode)

    MediaSession(SystemState *system, AudioFocus *audio, QObject *parent = nullptr);
    ~MediaSession() override;

    bool playing() const;
    QString source() const;
    bool bluetoothMode() const;
    Q_INVOKABLE void setBluetoothSource(const QString &name, bool active);
    QString title() const;
    QString artist() const;
    QString coverColor() const;
    int trackIndex() const;
    int position() const;
    int duration() const;
    int playMode() const;
    QVariantList tracks() const;
    QVariantList queue() const;
    QStringList lyrics() const;

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void playIndex(int index);
    Q_INVOKABLE bool playFile(const QString &path);
    Q_INVOKABLE void seek(int seconds);
    Q_INVOKABLE void setQueue(const QVariantList &indices);
    Q_INVOKABLE void playAll();
    Q_INVOKABLE void playInQueue(int trackIndex);
    Q_INVOKABLE void cyclePlayMode();
    Q_INVOKABLE void rescan();
    Q_INVOKABLE void applyRemoteNowPlaying(const QString &title, const QString &artist, bool playing,
                                           int positionSec, int durationSec);
    Q_INVOKABLE void clearRemoteNowPlaying();

signals:
    void playingChanged();
    void sourceChanged();
    void trackChanged();
    void positionChanged();
    void playModeChanged();
    void queueChanged();
    void tracksChanged();

private:
    struct Track {
        QString title;
        QString artist;
        QString file;
        QString color;
        int duration = 1;
        QStringList lyrics;
    };

    void loadTracks();
    void openCurrent();
    void applyVolume();
    void updateSource();
    void poll();
    void select(int index, bool start);
    void ensureQueue();
    void rebuildShuffle();
    int queuePosOf(int trackIndex) const;
    void advance(int delta, bool fromEnd);
    void loadPrefs();
    void savePrefs() const;
    static QStringList loadLyrics(const QString &wavPath, const QString &title);

    SystemState *m_system = nullptr;
    AudioFocus *m_audio = nullptr;
    bool m_btActive = false;
    QString m_btName;
    QVector<Track> m_tracks;
    QVector<int> m_queue;
    QVector<int> m_shuffle;
    int m_index = 0;
    int m_position = 0;
    int m_playMode = Loop;
    bool m_playing = false;
    bool m_advancing = false;
    bool m_remoteActive = false;
    bool m_remotePlaying = false;
    int m_remotePosition = 0;
    int m_remoteDuration = 1;
    int m_softMs = 0;
    QString m_source;
    QString m_remoteTitle;
    QString m_remoteArtist;
    QTimer m_timer;
};

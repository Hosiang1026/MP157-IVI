#pragma once

#include <QImage>
#include <QQuickPaintedItem>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVector>

class VideoScreen : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString clipName READ clipName WRITE setClipName NOTIFY clipNameChanged)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    Q_PROPERTY(bool preview READ preview WRITE setPreview NOTIFY previewChanged)
    Q_PROPERTY(int position READ position NOTIFY positionChanged)
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
public:
    explicit VideoScreen(QQuickItem *parent = nullptr);

    QString clipName() const;
    void setClipName(const QString &name);
    bool playing() const;
    void setPlaying(bool value);
    bool preview() const;
    void setPreview(bool value);
    int position() const;
    int duration() const;

    Q_INVOKABLE void seek(int seconds);
    Q_INVOKABLE QVariantList listClips() const;
    void paint(QPainter *painter) override;

signals:
    void clipNameChanged();
    void playingChanged();
    void previewChanged();
    void positionChanged();
    void durationChanged();
    void ended();

private:
    void load(const QString &path);
    void reload();
    void tick();

    QString m_clip;
    QVector<QImage> m_frames;
    int m_frame = 0;
    int m_fps = 8;
    int m_position = 0;
    bool m_playing = false;
    bool m_preview = false;
    QTimer m_timer;
};

#pragma once

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QUrl>

#include <atomic>

class FfmpegUrlPlayer : public QObject {
    Q_OBJECT
public:
    explicit FfmpegUrlPlayer(QObject *parent = nullptr);
    ~FfmpegUrlPlayer() override;

    QImage currentFrame() const;
    bool playing() const { return m_playing.load(); }
    QString lastError() const;

    void play(const QUrl &url);
    void pause();
    void resume();
    void stop();

signals:
    void frameChanged();
    void playingChanged();
    void errorOccurred(const QString &message);
    void finished();

private:
    void runLoop(const QString &url);
    void setPlaying(bool on);
    void pushFrame(const QImage &img);

    mutable QMutex m_frameMutex;
    QImage m_frame;
    QString m_error;
    std::atomic_bool m_playing{false};
    std::atomic_bool m_stop{false};
    std::atomic_bool m_paused{false};
    std::atomic_bool m_workerBusy{false};
};

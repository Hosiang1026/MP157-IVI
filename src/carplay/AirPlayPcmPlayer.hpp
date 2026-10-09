#pragma once

#include <QByteArray>
#include <QMutex>
#include <QObject>

class AirPlayPcmPlayer : public QObject {
    Q_OBJECT
public:
    explicit AirPlayPcmPlayer(QObject *parent = nullptr);
    ~AirPlayPcmPlayer() override;

    bool start(int sampleRate, int channels);
    void stop();
    void writePcm(const QByteArray &pcm);
    bool isRunning() const { return m_running; }

private:
    struct Impl;
    Impl *m = nullptr;
    bool m_running = false;
};

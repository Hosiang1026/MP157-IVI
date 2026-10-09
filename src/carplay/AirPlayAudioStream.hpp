#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUdpSocket>

#include <memory>

class AirPlayAudioStream : public QObject {
    Q_OBJECT
public:
    explicit AirPlayAudioStream(const QByteArray &key, int sampleRate, int channels, bool aac,
                                QObject *parent = nullptr);
    ~AirPlayAudioStream() override;

    bool start();
    void stop();
    quint16 dataPort() const { return m_dataPort; }
    quint16 controlPort() const { return m_controlPort; }

signals:
    void log(const QString &msg);
    void pcmReady(const QByteArray &pcm);

private slots:
    void onDataReady();
    void onControlReady();

private:
    void handleDatagram(const QByteArray &wire);

    QByteArray m_key;
    int m_sampleRate = 44100;
    int m_channels = 2;
    bool m_aac = false;
    quint16 m_dataPort = 0;
    quint16 m_controlPort = 0;
    std::unique_ptr<QUdpSocket> m_data;
    std::unique_ptr<QUdpSocket> m_control;
    void *m_aacDecoder = nullptr;
    int m_ok = 0;
    int m_fail = 0;
};

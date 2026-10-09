#pragma once

#include <QByteArray>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QVariantMap>

#include <memory>

class AirPlayBufferedAudio : public QObject {
    Q_OBJECT
public:
    static constexpr qint64 kBufferBytes = 8 * 1024 * 1024;

    explicit AirPlayBufferedAudio(const QByteArray &key, int sampleRate, int channels,
                                  QObject *parent = nullptr);
    ~AirPlayBufferedAudio() override;

    bool start();
    void stop();
    quint16 dataPort() const { return m_port; }
    int sampleRate() const { return m_sampleRate; }

    QVariantMap setRate(qint64 rtpTime, int rate);
    QVariantMap anchor() const;
    void flush(qint64 untilTs);

signals:
    void log(const QString &msg);
    void pcmReady(const QByteArray &pcm);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    void processBuffer();
    bool openFrame(const QByteArray &body, QByteArray *aacOut, qint64 *tsOut);
    QByteArray decodeAac(const QByteArray &aac);
    static QVariantMap makeAnchor(qint64 rtp, int rate);

    QByteArray m_key;
    int m_sampleRate = 48000;
    int m_channels = 2;
    quint16 m_port = 0;
    std::unique_ptr<QTcpServer> m_server;
    QTcpSocket *m_client = nullptr;
    QByteArray m_buf;
    int m_rate = 0;
    qint64 m_anchorRtp = -1;
    qint64 m_flushUntil = -1;
    int m_ok = 0;
    int m_fail = 0;
    void *m_aacDecoder = nullptr;
};

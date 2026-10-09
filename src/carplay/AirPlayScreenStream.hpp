#pragma once

#include <QByteArray>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

class AirPlayScreenStream : public QObject {
    Q_OBJECT
public:
    explicit AirPlayScreenStream(const QByteArray &key, QObject *parent = nullptr);
    ~AirPlayScreenStream() override;

    quint16 start();
    void stop();
    quint16 port() const { return m_port; }

signals:
    void config(const QByteArray &avcC);
    void frame(const QByteArray &annexB);
    void log(const QString &msg);
    void closed(const QString &reason);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void process();
    static QByteArray toAnnexB(QByteArray payload);

    QByteArray m_key;
    QTcpServer m_server;
    QTcpSocket *m_sock = nullptr;
    QByteArray m_buf;
    quint64 m_frameCounter = 0;
    quint16 m_port = 0;
};

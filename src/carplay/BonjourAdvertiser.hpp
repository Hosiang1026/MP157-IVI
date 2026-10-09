#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QMap>
#include <QNetworkInterface>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QUdpSocket>

class BonjourAdvertiser : public QObject {
    Q_OBJECT
public:
    explicit BonjourAdvertiser(QObject *parent = nullptr);
    ~BonjourAdvertiser() override;

    bool start(const QString &name,
               quint16 port,
               const QMap<QString, QString> &txt,
               const QHostAddress &bindAddress = QHostAddress::AnyIPv4,
               const QString &deviceId = {},
               const QString &sourceVersion = QStringLiteral("950.7.1"));
    void stop();
    void browseNow();
    void probeLanNeighbors();
    void probeHost(const QString &host, quint16 port = 49152);
    void setPreferredHost(const QString &host);
    QString preferredHost() const { return m_preferredHost; }
    bool isRunning() const { return m_running; }

signals:
    void log(const QString &msg);
    void failed(const QString &msg);
    void ctrlProbed(const QString &host, quint16 port, const QString &statusLine);
    void preferredHostInvalid(const QString &host);

private slots:
    void onReadyRead();
    void browseCarPlayCtrl();

private:
    void sendAnnouncement();
    void sendPtrQuery(const QString &serviceType);
    void sendDatagram(const QByteArray &pkt);
    QByteArray buildResponse(const QByteArray &query) const;
    void parseIncoming(const QByteArray &packet, const QHostAddress &sender);
    void probeCarPlayCtrl(const QHostAddress &host, quint16 port, bool preferred = false);
    void finishPreferredProbe(bool hit);
    QByteArray encodeDnsName(const QString &name) const;
    static quint16 readU16(const quint8 *p);
    static void writeU16(QByteArray &out, quint16 v);
    static void writeU32(QByteArray &out, quint32 v);
    static bool skipDnsName(const QByteArray &packet, int &off);
    static QString readDnsName(const QByteArray &packet, int off);

    QUdpSocket m_socket;
    QTimer m_browseTimer;
    bool m_running = false;
    QString m_instanceName;
    QString m_serviceType;
    quint16 m_port = 0;
    QHostAddress m_ipv4;
    QNetworkInterface m_iface;
    QMap<QString, QString> m_txt;
    QByteArray m_txtBlob;
    QString m_deviceId;
    QString m_sourceVersion;
    QString m_preferredHost;
    QSet<QString> m_probed;
    bool m_lanProbeBusy = false;
    bool m_preferWaiting = false;
    bool m_preferHit = false;
    int m_preferPending = 0;
    QTimer m_preferTimer;
};

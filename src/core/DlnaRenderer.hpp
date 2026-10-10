#pragma once

#include "FfmpegUrlPlayer.hpp"

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>

class AudioFocus;

class DlnaRenderer : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY detailChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY mediaChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY frameChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY mediaChanged)
    Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY mediaChanged)
    Q_PROPERTY(QString mediaUri READ mediaUri NOTIFY mediaChanged)
    Q_PROPERTY(QString transportState READ transportState NOTIFY mediaChanged)
    Q_PROPERTY(QString hostIp READ hostIp NOTIFY hostIpChanged)
    Q_PROPERTY(int httpPort READ httpPort NOTIFY runningChanged)
public:
    explicit DlnaRenderer(AudioFocus *audio, QObject *parent = nullptr);
    ~DlnaRenderer() override;

    QString status() const { return m_status; }
    QString detail() const { return m_detail; }
    bool running() const { return m_running; }
    bool hasMedia() const { return !m_mediaUri.isEmpty(); }
    bool hasFrame() const { return !currentImage().isNull(); }
    bool playing() const { return m_player.playing(); }
    QString mediaTitle() const { return m_mediaTitle; }
    QString mediaUri() const { return m_mediaUri; }
    QString transportState() const { return m_transportState; }
    QString hostIp() const { return m_hostIp; }
    int httpPort() const { return int(m_httpPort); }
    QImage currentImage() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void stopMedia();
    Q_INVOKABLE void pauseMedia();
    Q_INVOKABLE void resumeMedia();

signals:
    void statusChanged();
    void detailChanged();
    void runningChanged();
    void mediaChanged();
    void hostIpChanged();
    void frameChanged();

private slots:
    void onHttpNewConnection();
    void onHttpReadyRead();
    void onSsdpReadyRead();
    void onAnnounceTick();

private:
    struct HttpClient {
        QByteArray buffer;
    };

    void setStatus(const QString &status);
    void setDetail(const QString &detail);
    void setRunning(bool running);
    void setTransport(const QString &state);
    QString primaryIpv4() const;
    QNetworkInterface primaryIface() const;
    bool setupSsdpSocket();
    void sendSsdpNotify(const QString &nts);
    void replySsdpSearch(const QHostAddress &addr, quint16 port, const QByteArray &st);
    void sendSsdpResponse(const QHostAddress &addr, quint16 port, const QByteArray &st);
    void handleHttp(QTcpSocket *sock, const QByteArray &req);
    QByteArray deviceDescription() const;
    QByteArray scpdAvTransport() const;
    QByteArray scpdConnectionManager() const;
    QByteArray scpdRenderingControl() const;
    QByteArray soapResponse(const QString &service, const QString &action, const QString &body) const;
    QByteArray handleSoap(const QString &service, const QByteArray &action, const QByteArray &body);
    static QString xmlText(const QByteArray &xml, const char *tag);
    void applyUri(const QString &uri, const QString &meta);
    void fetchMedia(const QUrl &url);
    void beginPlayback(const QUrl &url);

    AudioFocus *m_audio = nullptr;
    FfmpegUrlPlayer m_player;
    QTcpServer m_http;
    QUdpSocket m_ssdp;
    QNetworkAccessManager m_nam;
    QTimer m_announce;
    QHash<QTcpSocket *, HttpClient> m_clients;
    QString m_uuid;
    QString m_status;
    QString m_detail;
    QString m_hostIp;
    QString m_mediaUri;
    QString m_mediaTitle;
    QString m_mediaMeta;
    QString m_transportState;
    QImage m_still;
    quint16 m_httpPort = 0;
    bool m_running = false;
    int m_volume = 50;
};

#pragma once

#include "AirPlayAudioStream.hpp"
#include "AirPlayBufferedAudio.hpp"
#include "AirPlayControlCipher.hpp"
#include "AirPlayIdentity.hpp"
#include "AirPlayPcmPlayer.hpp"
#include "AirPlayScreenStream.hpp"

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QImage>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMutex>
#include <QQueue>
#include <QElapsedTimer>
#include <QTimer>
#include <QUdpSocket>
#include <QVariantMap>

#include <atomic>
#include <limits>
#include <memory>

class LocalMfiAuth;

class AirPlayServer : public QObject {
    Q_OBJECT
public:
    explicit AirPlayServer(QObject *parent = nullptr);
    ~AirPlayServer() override;

    bool start(AirPlayIdentity *identity,
               LocalMfiAuth *mfi,
               const QString &deviceName,
               const QString &deviceId,
               const QString &model = QStringLiteral("MP157-IVI"),
               const QHostAddress &bindAddress = QHostAddress::AnyIPv4,
               const QString &sourceVersion = QStringLiteral("950.7.1"),
               const QString &bluetoothMac = {});
    void stop();

    quint16 port() const { return m_port; }
    bool isListening() const { return m_server.isListening(); }
    QImage videoFrame() const { return m_videoFrame; }
    int displayWidth() const { return m_displayW; }
    int displayHeight() const { return m_displayH; }
    void setDisplaySize(int width, int height);
    void setSafeAreaInsets(int top, int bottom, int left, int right);
    int safeAreaTop() const { return m_safeTop; }
    int safeAreaBottom() const { return m_safeBottom; }
    int safeAreaLeft() const { return m_safeLeft; }
    int safeAreaRight() const { return m_safeRight; }
    bool sendTouch(double xNorm, double yNorm, bool down);
    bool setNightMode(bool night);
    bool sendHardKey(const QString &key, bool down = true);
    bool sendLocation(double latitude, double longitude, double altitude, double speedMps,
                      double course, double accuracy);

    struct PairingStore {
        void save(const QString &id, const QByteArray &ltpk) { map[id] = ltpk; }
        QByteArray get(const QString &id) const { return map.value(id); }
        QHash<QString, QByteArray> map;
    };

signals:
    void listening(quint16 port);
    void clientConnected(const QString &peer);
    void sessionActive();
    void videoFrameChanged();
    void hostUiRequested();
    void nowPlayingInfo(const QString &title, const QString &artist, bool playing, int positionSec,
                        int durationSec);
    void navigationInfo(bool active, const QString &text, const QString &turn, int speedLimit, int etaMin,
                        const QString &destination);
    void telephonyInfo(bool active, bool ringing, const QString &name, const QString &number);
    void notificationInfo(const QString &appName, const QString &title, const QString &body);
    void log(const QString &msg);
    void failed(const QString &msg);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();
    void onEventNewConnection();
    void onEventReadyRead();
    void onEventDisconnected();
    void onScreenConfig(const QByteArray &avcC);
    void onScreenFrame(const QByteArray &annexB);
    void onDecodedFrame(const QImage &img, const QString &err);
    void drainDecodeQueue();
    void onTimingReadyRead();
    void onTimingTick();

private:
    struct ClientState;

    void handleRequest(ClientState *client, const QByteArray &method, const QString &path,
                       const QString &protocol, const QHash<QString, QString> &headers,
                       const QByteArray &body);
    void sendResponse(ClientState *client, const QString &protocol, int status,
                      const QHash<QString, QString> &headers, const QByteArray &body,
                      const QString &cseq);
    void processClientBuffer(ClientState *client);
    QByteArray buildInfoPlist() const;
    QByteArray handleSetup(ClientState *client, const QByteArray &body);
    quint16 ensureTimingUdp();
    quint16 ensureEventTcp();
    quint16 ensureKeepAliveUdp();
    QByteArray streamOutputKey(const QVariantMap &stream) const;
    QVariantMap setupAudioStream(const QVariantMap &stream, int type);
    QVariantMap setupBufferedAudio(const QVariantMap &stream);
    QVariantMap setupIapDataStream(const QVariantMap &stream);
    QByteArray handleBufferedControl(const QByteArray &method, const QByteArray &body);
    void pushVideoFrame(const QImage &image);
    void startTimingPeer(const QHostAddress &peer, quint16 peerPort);
    QByteArray ntpNowBytes() const;
    quint64 syncedNtp64() const;
    void handleTimingResponse(const QByteArray &msg);
    void resetNtpClock();
    bool sendEventCommand(const QVariantMap &command);
    bool flushEventOut();
    static QByteArray buildTouchReport(int x, int y, bool down);
    static QByteArray buildMediaReport(quint8 usageIndex);
    bool sendHidReport(const QString &uuid, const QByteArray &report);
    void handleIncomingCommand(const QVariantMap &cmd);
    static QString mapManeuverTurn(const QVariant &maneuver);
    static void audioFormatFromBits(quint64 bits, int *sampleRate, int *channels, bool *aac);

    QTcpServer m_server;
    QTcpServer m_eventServer;
    AirPlayIdentity *m_identity = nullptr;
    LocalMfiAuth *m_mfi = nullptr;
    QString m_deviceName;
    QString m_deviceId;
    QString m_model;
    QString m_sourceVersion = QStringLiteral("950.7.1");
    QString m_bluetoothMac;
    QHostAddress m_bindAddress = QHostAddress::AnyIPv4;
    quint16 m_port = 0;
    QHash<QTcpSocket *, ClientState *> m_clients;
    std::unique_ptr<PairingStore> m_pairings;
    std::unique_ptr<QUdpSocket> m_timingSock;
    std::unique_ptr<QUdpSocket> m_keepAliveSock;
    QTimer m_timingTimer;
    QElapsedTimer m_ntpMono;
    qint64 m_ntpClockOffsetNs = 0;
    bool m_ntpSynced = false;
    quint64 m_ntpPendingT1 = 0;
    bool m_ntpHasPendingT1 = false;
    double m_ntpDelays[8] = {};
    int m_ntpDelayIndex = 0;
    int m_ntpPickCount = 2;
    double m_ntpPickRtt = std::numeric_limits<double>::infinity();
    double m_ntpPickOffset = 0;
    QHostAddress m_timingPeer;
    quint16 m_timingPeerPort = 0;
    quint16 m_timingPort = 0;
    quint16 m_eventPort = 0;
    quint16 m_keepAlivePort = 0;
    QTcpSocket *m_eventSock = nullptr;
    QByteArray m_eventBuf;
    QByteArray m_eventPlain;
    QByteArray m_eventOut;
    std::unique_ptr<AirPlayControlCipher> m_eventCipher;
    QByteArray m_sharedSecret;
    std::unique_ptr<AirPlayScreenStream> m_screen;
    std::unique_ptr<AirPlayAudioStream> m_audio;
    std::unique_ptr<AirPlayBufferedAudio> m_buffered;
    std::unique_ptr<AirPlayPcmPlayer> m_pcm;
    std::unique_ptr<QTcpServer> m_iapServer;
    QList<QTcpSocket *> m_iapSocks;
    QByteArray m_sps;
    QByteArray m_pps;
    QImage m_videoFrame;
    int m_displayW = 1024;
    int m_displayH = 600;
    int m_safeTop = 0;
    int m_safeBottom = 0;
    int m_safeLeft = 0;
    int m_safeRight = 0;
    bool m_nightMode = false;
    int m_eventCseq = 0;
    qint64 m_lastTouchMs = 0;
    qint64 m_lastTouchSendMs = 0;
    double m_lastTouchX = 0.5;
    double m_lastTouchY = 0.5;
    bool m_touchDown = false;
    int m_lastTouchPx = -1;
    int m_lastTouchPy = -1;
    QMutex m_decodeMutex;
    QQueue<QByteArray> m_decodeQueue;
    std::atomic_bool m_decodeBusy{false};
    std::atomic_bool m_waitIdr{false};
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    void *m_mfDecoder = nullptr;
#endif
};

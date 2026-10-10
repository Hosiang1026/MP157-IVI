#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <atomic>

class BluetoothMediaHub;
class NotificationSession;

class BluetoothAncsClient : public QObject {
    Q_OBJECT
public:
    BluetoothAncsClient(BluetoothMediaHub *hub, NotificationSession *notifications,
                        QObject *parent = nullptr);
    ~BluetoothAncsClient() override;

private slots:
    void tick();
    void onBluezNotifProps(const QString &iface, const QVariantMap &changed,
                           const QStringList &invalidated);
    void onBluezDataProps(const QString &iface, const QVariantMap &changed,
                          const QStringList &invalidated);

private:
    void stopSession();
    void startSession(const QString &address);
    void handleNotificationSource(const QByteArray &value);
    void handleDataSource(const QByteArray &value);
    void requestAttributes(quint32 uid);
    void publish(const QString &appId, const QString &title, const QString &message);
    static QString friendlyApp(const QString &appId);

    BluetoothMediaHub *m_hub = nullptr;
    NotificationSession *m_notifications = nullptr;
    QTimer m_timer;
    QString m_address;
    QByteArray m_dataBuf;
    quint32 m_pendingUid = 0;
    bool m_haveApp = false;
    bool m_haveTitle = false;
    bool m_haveMessage = false;
    QString m_appId;
    QString m_title;
    QString m_message;
    std::atomic_bool m_starting{false};

    struct Platform;
    Platform *m_platform = nullptr;
};

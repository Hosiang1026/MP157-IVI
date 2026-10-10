#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <atomic>

class BluetoothMediaHub;
class NotificationSession;

class BluetoothMapClient : public QObject {
    Q_OBJECT
public:
    BluetoothMapClient(BluetoothMediaHub *hub, NotificationSession *notifications,
                       QObject *parent = nullptr);

private slots:
    void tick();

private:
    BluetoothMediaHub *m_hub = nullptr;
    NotificationSession *m_notifications = nullptr;
    QTimer m_timer;
    QString m_address;
    bool m_seeded = false;
    QSet<QString> m_seen;
    std::atomic_bool m_busy{false};
};

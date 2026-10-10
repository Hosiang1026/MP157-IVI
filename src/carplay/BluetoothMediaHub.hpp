#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

class BluetoothMediaHub : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QString activeAddress READ activeAddress NOTIFY activeChanged)
    Q_PROPERTY(QString activeName READ activeName NOTIFY activeChanged)
    Q_PROPERTY(bool activeConnected READ activeConnected NOTIFY activeChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
public:
    explicit BluetoothMediaHub(QObject *parent = nullptr);

    QVariantList devices() const;
    QString activeAddress() const;
    QString activeName() const;
    bool activeConnected() const;
    QString status() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool selectDevice(const QString &address);
    Q_INVOKABLE void clearActive();
    Q_INVOKABLE bool pairDevice(const QString &address);

signals:
    void devicesChanged();
    void activeChanged();
    void statusChanged();

private:
    void setStatus(const QString &status);
    void loadSettings();
    void saveSettings() const;
    void routePulseBluez(const QString &address) const;

    QVariantList m_devices;
    QString m_activeAddress;
    QString m_activeName;
    QString m_status;
    QTimer m_timer;
};

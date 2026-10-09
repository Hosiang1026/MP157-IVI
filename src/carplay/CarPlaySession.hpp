#pragma once

#include "AirPlayIdentity.hpp"
#include "AirPlayServer.hpp"
#include "BonjourAdvertiser.hpp"
#include "LocalMfiAuth.hpp"

#include <QImage>
#include <QObject>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QVariantList>

#include <atomic>

class CarPlaySession : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY detailChanged)
    Q_PROPERTY(bool identityReady READ identityReady NOTIFY identityReadyChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool canStart READ canStart NOTIFY canStartChanged)
    Q_PROPERTY(bool wifiConnected READ wifiConnected NOTIFY wifiChanged)
    Q_PROPERTY(QString wifiSsid READ wifiSsid NOTIFY wifiChanged)
    Q_PROPERTY(QString wifiPassword READ wifiPassword WRITE setWifiPassword NOTIFY wifiChanged)
    Q_PROPERTY(QString bluetoothAddress READ bluetoothAddress NOTIFY bluetoothChanged)
    Q_PROPERTY(QString bluetoothName READ bluetoothName NOTIFY bluetoothChanged)
    Q_PROPERTY(QVariantList wifiNetworks READ wifiNetworks NOTIFY wifiNetworksChanged)
    Q_PROPERTY(QVariantList bluetoothDevices READ bluetoothDevices NOTIFY bluetoothDevicesChanged)
    Q_PROPERTY(QImage videoFrame READ videoFrame NOTIFY videoFrameChanged)
    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY videoFrameChanged)
    Q_PROPERTY(int displayWidth READ displayWidth NOTIFY displaySizeChanged)
    Q_PROPERTY(int displayHeight READ displayHeight NOTIFY displaySizeChanged)
public:
    explicit CarPlaySession(QObject *parent = nullptr);
    ~CarPlaySession() override;

    QString status() const;
    QString detail() const;
    bool identityReady() const;
    bool running() const;
    bool canStart() const;
    bool wifiConnected() const;
    QString wifiSsid() const;
    QString wifiPassword() const;
    void setWifiPassword(const QString &password);
    QString bluetoothAddress() const;
    QString bluetoothName() const;
    QVariantList wifiNetworks() const;
    QVariantList bluetoothDevices() const;
    QImage videoFrame() const;
    bool hasVideo() const;
    int displayWidth() const;
    int displayHeight() const;

    Q_INVOKABLE void reloadIdentity();
    Q_INVOKABLE void refreshWifi();
    Q_INVOKABLE void refreshBluetooth();
    Q_INVOKABLE void selectWifi(const QString &ssid);
    Q_INVOKABLE bool connectWifi(const QString &ssid, const QString &password);
    Q_INVOKABLE void selectBluetooth(const QString &address, const QString &name);
    Q_INVOKABLE bool pairBluetooth(const QString &address);
    Q_INVOKABLE void setDisplaySize(int width, int height);
    Q_INVOKABLE void sendTouch(double xNorm, double yNorm, bool down);
    Q_INVOKABLE void start();
    Q_INVOKABLE void reconnectLast();
    Q_INVOKABLE void stop();

signals:
    void statusChanged();
    void detailChanged();
    void identityReadyChanged();
    void runningChanged();
    void canStartChanged();
    void wifiChanged();
    void bluetoothChanged();
    void wifiNetworksChanged();
    void bluetoothDevicesChanged();
    void videoFrameChanged();
    void displaySizeChanged();
    void hostUiRequested();

private:
    void setStatus(const QString &status);
    void setDetail(const QString &detail);
    void setRunning(bool running);
    void loadSettings();
    void saveSettings() const;
    void rememberPhoneIp(const QString &ip);
    void clearPhoneIp();
    void beginWireless();
    void stopInternal();
    void recoverStaleRunning();
    void emitCanStart();

    LocalMfiAuth m_mfi;
    AirPlayIdentity m_airPlayIdentity;
    AirPlayServer m_airPlay;
    BonjourAdvertiser m_bonjour;
    QThread *m_btThread = nullptr;
    QTimer m_airPlayWatchdog;
    QTimer m_airPlayNudge;
    std::atomic_bool m_airPlayUp{false};

    QString m_status;
    QString m_detail;
    QString m_wifiSsid;
    QString m_wifiPassword;
    QString m_lastPhoneIp;
    QString m_bluetoothAddress;
    QString m_bluetoothName;
    QString m_deviceId;
    QVariantList m_wifiNetworks;
    QVariantList m_bluetoothDevices;
    bool m_wifiConnected = false;
    bool m_running = false;
};

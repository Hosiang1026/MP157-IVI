#pragma once

#include "AoapTransport.hpp"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>

class GpsSource;
class VehicleState;

class AndroidAutoSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY detailChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY hasVideoChanged)
    Q_PROPERTY(bool demoMode READ demoMode NOTIFY hasVideoChanged)
    Q_PROPERTY(bool nightMode READ nightMode WRITE setNightMode NOTIFY nightModeChanged)
    Q_PROPERTY(QString aoapState READ aoapState NOTIFY aoapStateChanged)
    Q_PROPERTY(QString deviceLabel READ deviceLabel NOTIFY deviceLabelChanged)
    Q_PROPERTY(QString sensorSummary READ sensorSummary NOTIFY sensorsChanged)
    Q_PROPERTY(int bytesIn READ bytesIn NOTIFY trafficChanged)
    Q_PROPERTY(int bytesOut READ bytesOut NOTIFY trafficChanged)
public:
    explicit AndroidAutoSession(QObject *parent = nullptr);

    void setGpsSource(GpsSource *gps);
    void setVehicleState(VehicleState *vehicle);

    QString status() const { return m_status; }
    QString detail() const { return m_detail; }
    bool running() const { return m_running; }
    bool hasVideo() const { return m_hasVideo; }
    bool demoMode() const { return m_demoMode; }
    bool nightMode() const { return m_nightMode; }
    void setNightMode(bool night);
    QString aoapState() const { return m_aoap.stateName(); }
    QString deviceLabel() const { return m_aoap.deviceLabel(); }
    QString sensorSummary() const { return m_sensorSummary; }
    int bytesIn() const { return m_bytesIn; }
    int bytesOut() const { return m_bytesOut; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void startDemo();
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool sendHardKey(const QString &key, bool down = true);
    Q_INVOKABLE QVariantMap sensors() const;

signals:
    void statusChanged();
    void detailChanged();
    void runningChanged();
    void hasVideoChanged();
    void nightModeChanged();
    void aoapStateChanged();
    void deviceLabelChanged();
    void sensorsChanged();
    void trafficChanged();

private:
    void setStatus(const QString &status);
    void setDetail(const QString &detail);
    void setRunning(bool running);
    void setHasVideo(bool hasVideo);
    void onAoapReady(const QString &deviceLabel);
    void onAoapFailed(const QString &reason);
    void openLinkProbe();
    void openDemoProjection();
    void closeSession();
    void onPoll();
    void refreshSensors();

    AoapTransport m_aoap;
    QTimer m_poll;
    GpsSource *m_gps = nullptr;
    VehicleState *m_vehicle = nullptr;
    QString m_status;
    QString m_detail;
    QString m_sensorSummary;
    QVariantMap m_sensors;
    bool m_running = false;
    bool m_sessionOpen = false;
    bool m_hasVideo = false;
    bool m_demoMode = false;
    bool m_nightMode = false;
    int m_bytesIn = 0;
    int m_bytesOut = 0;
};

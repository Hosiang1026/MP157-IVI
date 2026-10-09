#pragma once

#include "AirPlayIdentity.hpp"
#include "AirPlayServer.hpp"
#include "BonjourAdvertiser.hpp"
#include "LocalMfiAuth.hpp"

#include <QImage>
#include <QObject>
#include <QString>

class AirPlayMirrorSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY detailChanged)
    Q_PROPERTY(bool identityReady READ identityReady NOTIFY identityReadyChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY videoFrameChanged)
    Q_PROPERTY(QString hostIp READ hostIp NOTIFY hostIpChanged)
    Q_PROPERTY(int port READ port NOTIFY runningChanged)
public:
    explicit AirPlayMirrorSession(QObject *parent = nullptr);
    ~AirPlayMirrorSession() override;

    QString status() const { return m_status; }
    QString detail() const { return m_detail; }
    bool identityReady() const { return m_mfi.isReady(); }
    bool running() const { return m_running; }
    bool hasVideo() const { return !m_airPlay.videoFrame().isNull(); }
    QString hostIp() const { return m_hostIp; }
    int port() const { return int(m_airPlay.port()); }
    QImage videoFrame() const { return m_airPlay.videoFrame(); }

    Q_INVOKABLE void reloadIdentity();
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

signals:
    void statusChanged();
    void detailChanged();
    void identityReadyChanged();
    void runningChanged();
    void videoFrameChanged();
    void hostIpChanged();

private:
    void setStatus(const QString &status);
    void setDetail(const QString &detail);
    void setRunning(bool running);

    LocalMfiAuth m_mfi;
    AirPlayIdentity m_identity;
    AirPlayServer m_airPlay;
    BonjourAdvertiser m_bonjour;
    QString m_status;
    QString m_detail;
    QString m_hostIp;
    QString m_deviceId;
    bool m_running = false;
};

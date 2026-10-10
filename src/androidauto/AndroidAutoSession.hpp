#pragma once

#include "AoapTransport.hpp"

#include <QObject>
#include <QString>
#include <QTimer>

class AndroidAutoSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY detailChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY hasVideoChanged)
    Q_PROPERTY(QString aoapState READ aoapState NOTIFY aoapStateChanged)
    Q_PROPERTY(QString deviceLabel READ deviceLabel NOTIFY deviceLabelChanged)
    Q_PROPERTY(int bytesIn READ bytesIn NOTIFY trafficChanged)
    Q_PROPERTY(int bytesOut READ bytesOut NOTIFY trafficChanged)
public:
    explicit AndroidAutoSession(QObject *parent = nullptr);

    QString status() const { return m_status; }
    QString detail() const { return m_detail; }
    bool running() const { return m_running; }
    bool hasVideo() const { return false; }
    QString aoapState() const { return m_aoap.stateName(); }
    QString deviceLabel() const { return m_aoap.deviceLabel(); }
    int bytesIn() const { return m_bytesIn; }
    int bytesOut() const { return m_bytesOut; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

signals:
    void statusChanged();
    void detailChanged();
    void runningChanged();
    void hasVideoChanged();
    void aoapStateChanged();
    void deviceLabelChanged();
    void trafficChanged();

private:
    void setStatus(const QString &status);
    void setDetail(const QString &detail);
    void setRunning(bool running);
    void onAoapReady(const QString &deviceLabel);
    void onAoapFailed(const QString &reason);
    void openLinkProbe();
    void closeSession();
    void onPoll();

    AoapTransport m_aoap;
    QTimer m_poll;
    QString m_status;
    QString m_detail;
    bool m_running = false;
    bool m_sessionOpen = false;
    int m_bytesIn = 0;
    int m_bytesOut = 0;
};

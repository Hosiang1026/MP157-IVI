#pragma once

#include "AoapDefs.hpp"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTimer>

class AoapTransport : public QObject {
    Q_OBJECT
public:
    explicit AoapTransport(QObject *parent = nullptr);
    ~AoapTransport() override;

    Aoap::State state() const { return m_state; }
    QString stateName() const;
    QString detail() const { return m_detail; }
    QString deviceLabel() const { return m_deviceLabel; }
    bool watching() const { return m_watching; }
    bool accessoryOpen() const;

    void startWatch();
    void stop();

    qint64 writeBulk(const QByteArray &data);
    QByteArray readBulk(int maxBytes, int timeoutMs = 200);

signals:
    void stateChanged();
    void detailChanged();
    void deviceLabelChanged();
    void accessoryReady(const QString &deviceLabel);
    void failed(const QString &reason);
    void bulkActivity(int bytesIn, int bytesOut);

private:
    struct UsbDev {
        QString path;
        quint16 vid = 0;
        quint16 pid = 0;
        QString name;
        bool accessory = false;
    };

    void setState(Aoap::State state);
    void setDetail(const QString &detail);
    void setDeviceLabel(const QString &label);
    void tickWatch();
    void closeHandles();

    QList<UsbDev> enumerateDevices() const;
    bool isAccessoryPid(quint16 vid, quint16 pid) const;
    bool probeOnce();
    bool switchToAccessory(const UsbDev &dev);
    bool openAccessory(const UsbDev &dev);
    bool controlTransfer(const QString &path, quint8 reqType, quint8 request, quint16 value,
                         quint16 index, QByteArray *data, int timeoutMs = 2000);
    bool sendAoapStrings(const QString &path);
    bool claimAccessoryInterface(const QString &path);

    Aoap::State m_state = Aoap::State::Idle;
    QString m_detail;
    QString m_deviceLabel;
    QString m_openPath;
    bool m_watching = false;
    int m_epIn = -1;
    int m_epOut = -1;
    int m_iface = 0;
    QTimer m_watchTimer;
#ifdef Q_OS_WIN
    void *m_winHandle = nullptr;
    void *m_winUsb = nullptr;
#endif
#ifdef Q_OS_LINUX
    int m_fd = -1;
#endif
};

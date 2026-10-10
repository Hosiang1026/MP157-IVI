#pragma once

#include <QFile>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include <atomic>

class VehicleState;

class CameraService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY frameChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY stateChanged)
    Q_PROPERTY(bool reverseActive READ reverseActive NOTIFY stateChanged)
    Q_PROPERTY(bool demoMode READ demoMode NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY stateChanged)
    Q_PROPERTY(QStringList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(int recordSeconds READ recordSeconds NOTIFY stateChanged)
public:
    explicit CameraService(VehicleState *vehicle = nullptr, QObject *parent = nullptr);
    ~CameraService() override;

    bool active() const { return m_active.load(); }
    bool hasFrame() const;
    bool recording() const { return m_recording.load(); }
    bool reverseActive() const { return m_holders.contains(QStringLiteral("reverse")); }
    bool demoMode() const { return m_demoMode; }
    QString status() const { return m_status; }
    QString deviceName() const { return m_deviceName; }
    QStringList devices() const { return m_devices; }
    int recordSeconds() const { return m_recordSeconds; }
    QImage currentFrame() const;

    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void setDevice(const QString &name);
    Q_INVOKABLE void acquire(const QString &holder);
    Q_INVOKABLE void release(const QString &holder);
    Q_INVOKABLE void dismissReverse();
    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE QVariantList listRecordings() const;

signals:
    void frameChanged();
    void stateChanged();
    void devicesChanged();

private:
    void onVehicleChanged();
    void ensureRunning();
    void stopCapture();
    void demoLoop();
    void pushFrame(const QImage &img);
    void setStatus(const QString &text);
    bool openRealCapture(const QString &device);
    QString recordingsDir() const;
    void writeRecordFrame(const QImage &img);
    void finalizeAvi();

    VehicleState *m_vehicle = nullptr;
    mutable QMutex m_frameMutex;
    QImage m_frame;
    QString m_status = QStringLiteral("未启动");
    QString m_deviceName;
    QStringList m_devices;
    QStringList m_holders;
    bool m_reverseDismissed = false;
    bool m_demoMode = false;
    int m_recordSeconds = 0;
    QTimer m_recordTick;
    QString m_recordPath;
    QMutex m_recordMutex;
    QByteArray m_aviIndex;
    qint64 m_aviMoviSizePos = 0;
    int m_aviFrames = 0;
    int m_aviW = 0;
    int m_aviH = 0;
    QFile *m_aviFile = nullptr;

    std::atomic_bool m_active{false};
    std::atomic_bool m_recording{false};
    std::atomic_bool m_stop{false};
    std::atomic_bool m_workerBusy{false};
};

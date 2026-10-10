#pragma once

#include <QFile>
#include <QObject>
#include <QString>
#include <QTimer>

class GpsSource : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(bool hasFix READ hasFix NOTIFY changed)
    Q_PROPERTY(QString device READ device WRITE setDevice NOTIFY changed)
    Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate NOTIFY changed)
    Q_PROPERTY(double latitude READ latitude NOTIFY changed)
    Q_PROPERTY(double longitude READ longitude NOTIFY changed)
    Q_PROPERTY(double altitude READ altitude NOTIFY changed)
    Q_PROPERTY(double speedMps READ speedMps NOTIFY changed)
    Q_PROPERTY(double course READ course NOTIFY changed)
    Q_PROPERTY(double accuracy READ accuracy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool demoMode READ demoMode WRITE setDemoMode NOTIFY changed)
public:
    explicit GpsSource(QObject *parent = nullptr);

    bool enabled() const;
    void setEnabled(bool value);
    bool hasFix() const;
    QString device() const;
    void setDevice(const QString &device);
    int baudRate() const;
    void setBaudRate(int baud);
    double latitude() const;
    double longitude() const;
    double altitude() const;
    double speedMps() const;
    double course() const;
    double accuracy() const;
    QString status() const;
    bool demoMode() const;
    void setDemoMode(bool value);

    Q_INVOKABLE void setDemoPosition(double lat, double lon);
    Q_INVOKABLE void refresh();

signals:
    void changed();
    void positionUpdated();

private slots:
    void onTick();

private:
    void loadSettings();
    void saveSettings() const;
    void openDevice();
    void closeDevice();
    void pollDevice();
    void configureBaud() const;
    void parseNmeaLine(const QString &line);
    static bool parseLatLon(const QString &lat, const QString &ns, const QString &lon, const QString &ew,
                            double *outLat, double *outLon);
    void applyFix(double lat, double lon, double alt, double speedKnots, double course, bool valid);

    QFile m_port;
    QTimer m_timer;
    QByteArray m_buf;
    QString m_device;
    QString m_status = QStringLiteral("未启用");
    int m_baudRate = 9600;
    bool m_enabled = false;
    bool m_demoMode = true;
    bool m_hasFix = false;
    double m_lat = 30.2741;
    double m_lon = 120.1551;
    double m_alt = 10.0;
    double m_speedMps = 0.0;
    double m_course = 0.0;
    double m_accuracy = 25.0;
};

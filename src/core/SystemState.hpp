#pragma once

#include <atomic>

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariant>

class SystemState : public QObject {
    Q_OBJECT
    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool bluetooth READ bluetooth WRITE setBluetooth NOTIFY bluetoothChanged)
    Q_PROPERTY(bool wifi READ wifi WRITE setWifi NOTIFY wifiChanged)
    Q_PROPERTY(QString wifiName READ wifiName NOTIFY wifiNameChanged)
    Q_PROPERTY(int wifiSignal READ wifiSignal NOTIFY wifiSignalChanged)
    Q_PROPERTY(int battery READ battery NOTIFY batteryChanged)
    Q_PROPERTY(QString time READ time NOTIFY timeChanged)
    Q_PROPERTY(bool autoTheme READ autoTheme WRITE setAutoTheme NOTIFY autoThemeChanged)
    Q_PROPERTY(bool manualDark READ manualDark WRITE setManualDark NOTIFY manualDarkChanged)
    Q_PROPERTY(bool dark READ dark NOTIFY darkChanged)
    Q_PROPERTY(QString page READ page NOTIFY darkChanged)
    Q_PROPERTY(QString card READ card NOTIFY darkChanged)
    Q_PROPERTY(QString ink READ ink NOTIFY darkChanged)
    Q_PROPERTY(QString secondary READ secondary NOTIFY darkChanged)
    Q_PROPERTY(QString fill READ fill NOTIFY darkChanged)
    Q_PROPERTY(QString highlight READ highlight NOTIFY darkChanged)
    Q_PROPERTY(QString tint READ tint NOTIFY darkChanged)
    Q_PROPERTY(QString separator READ separator NOTIFY darkChanged)
    Q_PROPERTY(QString selected READ selected NOTIFY darkChanged)
    Q_PROPERTY(QString elevated READ elevated NOTIFY darkChanged)
    Q_PROPERTY(QString danger READ danger CONSTANT)
    Q_PROPERTY(QString success READ success CONSTANT)
    Q_PROPERTY(QString warning READ warning CONSTANT)
    Q_PROPERTY(bool developerMode READ developerMode WRITE setDeveloperMode NOTIFY developerModeChanged)
    Q_PROPERTY(int lockTimeout READ lockTimeout WRITE setLockTimeout NOTIFY lockTimeoutChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
public:
    explicit SystemState(QObject *parent = nullptr);

    qreal brightness() const;
    void setBrightness(qreal value);
    qreal volume() const;
    void setVolume(qreal value);
    bool bluetooth() const;
    void setBluetooth(bool value);
    bool wifi() const;
    void setWifi(bool value);
    QString wifiName() const;
    int wifiSignal() const;
    int battery() const;
    QString time() const;
    bool autoTheme() const;
    void setAutoTheme(bool value);
    bool manualDark() const;
    void setManualDark(bool value);
    bool dark() const;
    QString page() const;
    QString card() const;
    QString ink() const;
    QString secondary() const;
    QString fill() const;
    QString highlight() const;
    QString tint() const;
    QString separator() const;
    QString selected() const;
    QString elevated() const;
    QString danger() const;
    QString success() const;
    QString warning() const;
    bool developerMode() const;
    void setDeveloperMode(bool value);
    int lockTimeout() const;
    void setLockTimeout(int minutes);
    QString appVersion() const;

    Q_INVOKABLE void unlockDeveloper();
    Q_INVOKABLE QVariant pref(const QString &key, const QVariant &fallback = QVariant()) const;
    Q_INVOKABLE void setPref(const QString &key, const QVariant &value);

signals:
    void brightnessChanged();
    void volumeChanged();
    void bluetoothChanged();
    void wifiChanged();
    void wifiNameChanged();
    void wifiSignalChanged();
    void batteryChanged();
    void timeChanged();
    void autoThemeChanged();
    void manualDarkChanged();
    void darkChanged();
    void developerModeChanged();
    void lockTimeoutChanged();

private slots:
    void applyLinkResults(int battery, const QString &name, int signal);

private:
    void updateTime();
    void refreshLink();
    void refreshDark();
    bool nightNow() const;

    qreal m_brightness = 0.85;
    qreal m_volume = 0.5;
    bool m_bluetooth = true;
    bool m_wifi = true;
    QString m_wifiName;
    int m_wifiSignal = 0;
    int m_battery = -1;
    QString m_time;
    bool m_autoTheme = true;
    bool m_manualDark = false;
    bool m_dark = false;
    bool m_developerMode = false;
    int m_lockTimeout = 5;
    int m_devTaps = 0;
    QTimer m_timer;
    QTimer m_linkTimer;
    std::atomic<bool> m_linkBusy{false};
};

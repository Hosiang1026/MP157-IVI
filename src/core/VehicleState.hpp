#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVector>

class WeatherService;

class VehicleState : public QObject {
    Q_OBJECT
    Q_PROPERTY(int speed READ speed NOTIFY changed)
    Q_PROPERTY(QString gear READ gear NOTIFY changed)
    Q_PROPERTY(int outsideTemp READ outsideTemp NOTIFY changed)
    Q_PROPERTY(int coolant READ coolant NOTIFY changed)
    Q_PROPERTY(int fuel READ fuel NOTIFY changed)
    Q_PROPERTY(int rangeKm READ rangeKm NOTIFY changed)
    Q_PROPERTY(int odometer READ odometer NOTIFY changed)
    Q_PROPERTY(int trip READ trip NOTIFY changed)
    Q_PROPERTY(bool locked READ locked NOTIFY changed)
    Q_PROPERTY(bool lights READ lights NOTIFY changed)
    Q_PROPERTY(qreal tireFl READ tireFl NOTIFY changed)
    Q_PROPERTY(qreal tireFr READ tireFr NOTIFY changed)
    Q_PROPERTY(qreal tireRl READ tireRl NOTIFY changed)
    Q_PROPERTY(qreal tireRr READ tireRr NOTIFY changed)
    Q_PROPERTY(bool tireAlert READ tireAlert NOTIFY changed)
    Q_PROPERTY(bool seatbeltOn READ seatbeltOn NOTIFY changed)
    Q_PROPERTY(bool handbrakeOn READ handbrakeOn NOTIFY changed)
    Q_PROPERTY(bool oilPressureLow READ oilPressureLow NOTIFY changed)
    Q_PROPERTY(qreal batteryVoltage READ batteryVoltage NOTIFY changed)
    Q_PROPERTY(bool batteryLow READ batteryLow NOTIFY changed)
    Q_PROPERTY(QVariantList batteryHistory READ batteryHistory NOTIFY changed)
    Q_PROPERTY(qreal batteryHistoryMin READ batteryHistoryMin NOTIFY changed)
    Q_PROPERTY(qreal batteryHistoryMax READ batteryHistoryMax NOTIFY changed)
    Q_PROPERTY(bool brakeWear READ brakeWear NOTIFY changed)
    Q_PROPERTY(bool engineFault READ engineFault NOTIFY changed)
    Q_PROPERTY(bool absFault READ absFault NOTIFY changed)
    Q_PROPERTY(bool espFault READ espFault NOTIFY changed)
    Q_PROPERTY(bool coolantLow READ coolantLow NOTIFY changed)
    Q_PROPERTY(bool washerLow READ washerLow NOTIFY changed)
    Q_PROPERTY(bool doorAjar READ doorAjar NOTIFY changed)
    Q_PROPERTY(bool hoodOpen READ hoodOpen NOTIFY changed)
    Q_PROPERTY(bool trunkOpen READ trunkOpen NOTIFY changed)
    Q_PROPERTY(bool serviceDue READ serviceDue NOTIFY changed)
    Q_PROPERTY(QString alertText READ alertText NOTIFY changed)
    Q_PROPERTY(QString alertColor READ alertColor NOTIFY changed)
    Q_PROPERTY(int alertCount READ alertCount NOTIFY changed)
    Q_PROPERTY(int alertIndex READ alertIndex NOTIFY changed)
public:
    static constexpr int kBatteryHistoryMax = 336;

    struct AlertItem {
        QString text;
        QString color;
    };

    explicit VehicleState(WeatherService *weather = nullptr, QObject *parent = nullptr);

    int speed() const;
    QString gear() const;
    int outsideTemp() const;
    int coolant() const;
    int fuel() const;
    int rangeKm() const;
    int odometer() const;
    int trip() const;
    bool locked() const;
    bool lights() const;
    qreal tireFl() const;
    qreal tireFr() const;
    qreal tireRl() const;
    qreal tireRr() const;
    bool tireAlert() const;
    bool seatbeltOn() const;
    bool handbrakeOn() const;
    bool oilPressureLow() const;
    qreal batteryVoltage() const;
    bool batteryLow() const;
    QVariantList batteryHistory() const;
    qreal batteryHistoryMin() const;
    qreal batteryHistoryMax() const;
    bool brakeWear() const;
    bool engineFault() const;
    bool absFault() const;
    bool espFault() const;
    bool coolantLow() const;
    bool washerLow() const;
    bool doorAjar() const;
    bool hoodOpen() const;
    bool trunkOpen() const;
    bool serviceDue() const;
    QString alertText() const;
    QString alertColor() const;
    int alertCount() const;
    int alertIndex() const;

    Q_INVOKABLE void setGear(const QString &gear);
    Q_INVOKABLE void toggleLocked();
    Q_INVOKABLE void toggleLights();
    Q_INVOKABLE void toggleSeatbelt();
    Q_INVOKABLE void toggleHandbrake();
    Q_INVOKABLE void toggleDoorAjar();
    Q_INVOKABLE void cycleHoodTrunk();
    Q_INVOKABLE void resetTrip();

signals:
    void changed();

private:
    void tick();
    void syncWeather();
    void persist() const;
    void load();
    void advanceAlert();
    void sampleBattery();
    void pushBatterySample(qreal v);
    void seedBatteryHistory();
    QVector<AlertItem> activeAlerts() const;

    WeatherService *m_weather = nullptr;
    int m_speed = 36;
    int m_dir = 1;
    QString m_gear = QStringLiteral("D");
    int m_outsideTemp = 26;
    int m_coolant = 90;
    int m_fuel = 62;
    int m_rangeKm = 380;
    int m_odometer = 18640;
    int m_trip = 26;
    qreal m_distanceAcc = 0;
    bool m_locked = true;
    bool m_lights = false;
    qreal m_tireFl = 2.4;
    qreal m_tireFr = 2.3;
    qreal m_tireRl = 2.4;
    qreal m_tireRr = 2.2;
    bool m_seatbeltOn = true;
    bool m_handbrakeOn = false;
    bool m_oilPressureLow = false;
    qreal m_batteryVoltage = 13.8;
    qreal m_batteryNoise = 0;
    QVector<qreal> m_batteryHistory;
    qreal m_batteryHistoryMin = 11.5;
    qreal m_batteryHistoryMax = 14.8;
    int m_batterySampleTick = 0;
    bool m_brakeWear = false;
    bool m_engineFault = false;
    bool m_absFault = false;
    bool m_espFault = false;
    bool m_coolantLow = false;
    bool m_washerLow = false;
    bool m_doorAjar = false;
    bool m_hoodOpen = false;
    bool m_trunkOpen = false;
    int m_alertIndex = 0;
    QTimer m_timer;
    QTimer m_alertTimer;
};

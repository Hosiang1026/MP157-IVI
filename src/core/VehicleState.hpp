#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

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
public:
    explicit VehicleState(QObject *parent = nullptr);

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

    Q_INVOKABLE void setGear(const QString &gear);
    Q_INVOKABLE void toggleLocked();
    Q_INVOKABLE void toggleLights();
    Q_INVOKABLE void resetTrip();

signals:
    void changed();

private:
    int m_speed = 36;
    int m_dir = 1;
    QString m_gear = QStringLiteral("D");
    int m_outsideTemp = 26;
    int m_coolant = 90;
    int m_fuel = 62;
    int m_rangeKm = 380;
    int m_odometer = 18640;
    int m_trip = 26;
    bool m_locked = true;
    bool m_lights = false;
    qreal m_tireFl = 2.4;
    qreal m_tireFr = 2.3;
    qreal m_tireRl = 2.4;
    qreal m_tireRr = 2.2;
    QTimer m_timer;
};

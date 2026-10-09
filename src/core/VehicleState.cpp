#include "VehicleState.hpp"

VehicleState::VehicleState(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (m_gear != QStringLiteral("D"))
            return;
        m_speed += m_dir;
        if (m_speed >= 58 || m_speed <= 22)
            m_dir = -m_dir;
        emit changed();
    });
    m_timer.start();
}

int VehicleState::speed() const { return m_speed; }
QString VehicleState::gear() const { return m_gear; }
int VehicleState::outsideTemp() const { return m_outsideTemp; }
int VehicleState::coolant() const { return m_coolant; }
int VehicleState::fuel() const { return m_fuel; }
int VehicleState::rangeKm() const { return m_rangeKm; }
int VehicleState::odometer() const { return m_odometer; }
int VehicleState::trip() const { return m_trip; }
bool VehicleState::locked() const { return m_locked; }
bool VehicleState::lights() const { return m_lights; }
qreal VehicleState::tireFl() const { return m_tireFl; }
qreal VehicleState::tireFr() const { return m_tireFr; }
qreal VehicleState::tireRl() const { return m_tireRl; }
qreal VehicleState::tireRr() const { return m_tireRr; }

void VehicleState::setGear(const QString &gear)
{
    if (gear == m_gear || (gear != QStringLiteral("P") && gear != QStringLiteral("R") && gear != QStringLiteral("N") && gear != QStringLiteral("D")))
        return;
    m_gear = gear;
    if (gear == QStringLiteral("P") || gear == QStringLiteral("N"))
        m_speed = 0;
    else if (gear == QStringLiteral("R"))
        m_speed = 6;
    else if (m_speed < 20)
        m_speed = 32;
    emit changed();
}

void VehicleState::toggleLocked()
{
    m_locked = !m_locked;
    emit changed();
}

void VehicleState::toggleLights()
{
    m_lights = !m_lights;
    emit changed();
}

void VehicleState::resetTrip()
{
    m_trip = 0;
    emit changed();
}

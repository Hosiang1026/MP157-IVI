#include "VehicleState.hpp"

#include "WeatherService.hpp"

#include <algorithm>
#include <QRandomGenerator>
#include <QSettings>
#include <QtMath>

VehicleState::VehicleState(WeatherService *weather, QObject *parent)
    : QObject(parent)
    , m_weather(weather)
{
    load();
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &VehicleState::tick);
    m_timer.start();
    m_alertTimer.setInterval(2500);
    connect(&m_alertTimer, &QTimer::timeout, this, &VehicleState::advanceAlert);
    m_alertTimer.start();
    if (m_weather)
        connect(m_weather, &WeatherService::updated, this, &VehicleState::syncWeather);
    syncWeather();
}

void VehicleState::load()
{
    QSettings s;
    s.beginGroup(QStringLiteral("vehicle"));
    m_fuel = qBound(5, s.value(QStringLiteral("fuel"), m_fuel).toInt(), 100);
    m_odometer = qMax(0, s.value(QStringLiteral("odometer"), m_odometer).toInt());
    m_trip = qMax(0, s.value(QStringLiteral("trip"), m_trip).toInt());
    m_locked = s.value(QStringLiteral("locked"), m_locked).toBool();
    m_lights = s.value(QStringLiteral("lights"), m_lights).toBool();
    m_seatbeltOn = s.value(QStringLiteral("seatbeltOn"), m_seatbeltOn).toBool();
    m_handbrakeOn = s.value(QStringLiteral("handbrakeOn"), m_handbrakeOn).toBool();
    m_oilPressureLow = s.value(QStringLiteral("oilPressureLow"), m_oilPressureLow).toBool();
    m_batteryVoltage = qBound(10.0, s.value(QStringLiteral("batteryVoltage"), m_batteryVoltage).toDouble(), 15.5);
    m_brakeWear = s.value(QStringLiteral("brakeWear"), m_brakeWear).toBool();
    m_engineFault = s.value(QStringLiteral("engineFault"), m_engineFault).toBool();
    m_absFault = s.value(QStringLiteral("absFault"), m_absFault).toBool();
    m_espFault = s.value(QStringLiteral("espFault"), m_espFault).toBool();
    m_coolantLow = s.value(QStringLiteral("coolantLow"), m_coolantLow).toBool();
    m_washerLow = s.value(QStringLiteral("washerLow"), m_washerLow).toBool();
    m_doorAjar = s.value(QStringLiteral("doorAjar"), m_doorAjar).toBool();
    m_hoodOpen = s.value(QStringLiteral("hoodOpen"), m_hoodOpen).toBool();
    m_trunkOpen = s.value(QStringLiteral("trunkOpen"), m_trunkOpen).toBool();
    m_rangeKm = qMax(20, qRound(m_fuel * 6.1));

    const QVariantList hist = s.value(QStringLiteral("batteryHistory")).toList();
    m_batteryHistory.clear();
    m_batteryHistory.reserve(hist.size());
    for (const QVariant &v : hist) {
        const qreal x = v.toDouble();
        if (x >= 10.0 && x <= 16.0)
            m_batteryHistory.append(x);
    }
    if (m_batteryHistory.size() > kBatteryHistoryMax)
        m_batteryHistory = m_batteryHistory.mid(m_batteryHistory.size() - kBatteryHistoryMax);
    if (m_batteryHistory.isEmpty())
        seedBatteryHistory();
    else {
        m_batteryHistoryMin = *std::min_element(m_batteryHistory.cbegin(), m_batteryHistory.cend());
        m_batteryHistoryMax = *std::max_element(m_batteryHistory.cbegin(), m_batteryHistory.cend());
    }
    s.endGroup();
}

void VehicleState::persist() const
{
    QSettings s;
    s.beginGroup(QStringLiteral("vehicle"));
    s.setValue(QStringLiteral("fuel"), m_fuel);
    s.setValue(QStringLiteral("odometer"), m_odometer);
    s.setValue(QStringLiteral("trip"), m_trip);
    s.setValue(QStringLiteral("locked"), m_locked);
    s.setValue(QStringLiteral("lights"), m_lights);
    s.setValue(QStringLiteral("seatbeltOn"), m_seatbeltOn);
    s.setValue(QStringLiteral("handbrakeOn"), m_handbrakeOn);
    s.setValue(QStringLiteral("oilPressureLow"), m_oilPressureLow);
    s.setValue(QStringLiteral("batteryVoltage"), m_batteryVoltage);
    s.setValue(QStringLiteral("brakeWear"), m_brakeWear);
    s.setValue(QStringLiteral("engineFault"), m_engineFault);
    s.setValue(QStringLiteral("absFault"), m_absFault);
    s.setValue(QStringLiteral("espFault"), m_espFault);
    s.setValue(QStringLiteral("coolantLow"), m_coolantLow);
    s.setValue(QStringLiteral("washerLow"), m_washerLow);
    s.setValue(QStringLiteral("doorAjar"), m_doorAjar);
    s.setValue(QStringLiteral("hoodOpen"), m_hoodOpen);
    s.setValue(QStringLiteral("trunkOpen"), m_trunkOpen);

    QVariantList hist;
    hist.reserve(m_batteryHistory.size());
    for (qreal v : m_batteryHistory)
        hist.append(v);
    s.setValue(QStringLiteral("batteryHistory"), hist);
    s.endGroup();
}

void VehicleState::seedBatteryHistory()
{
    m_batteryHistory.clear();
    m_batteryHistory.reserve(96);
    qreal v = 12.6;
    for (int i = 0; i < 96; ++i) {
        if (i < 24)
            v = 12.55 + (QRandomGenerator::global()->generateDouble() - 0.5) * 0.08;
        else if (i < 72)
            v = 13.9 + (QRandomGenerator::global()->generateDouble() - 0.5) * 0.25;
        else
            v = 12.45 + (QRandomGenerator::global()->generateDouble() - 0.5) * 0.12;
        m_batteryHistory.append(qBound(11.8, v, 14.6));
    }
    m_batteryHistoryMin = *std::min_element(m_batteryHistory.cbegin(), m_batteryHistory.cend());
    m_batteryHistoryMax = *std::max_element(m_batteryHistory.cbegin(), m_batteryHistory.cend());
    m_batteryVoltage = m_batteryHistory.last();
}

void VehicleState::pushBatterySample(qreal v)
{
    v = qBound(10.0, v, 15.5);
    m_batteryHistory.append(v);
    if (m_batteryHistory.size() > kBatteryHistoryMax)
        m_batteryHistory.removeFirst();
    m_batteryHistoryMin = *std::min_element(m_batteryHistory.cbegin(), m_batteryHistory.cend());
    m_batteryHistoryMax = *std::max_element(m_batteryHistory.cbegin(), m_batteryHistory.cend());
}

void VehicleState::sampleBattery()
{
    qreal target = 12.5;
    if (m_gear == QStringLiteral("D") || m_gear == QStringLiteral("R") || m_gear == QStringLiteral("N"))
        target = m_lights ? 13.6 : 14.0;
    else if (m_lights)
        target = 12.1;

    m_batteryNoise += (QRandomGenerator::global()->generateDouble() - 0.5) * 0.04;
    m_batteryNoise = qBound(-0.12, m_batteryNoise, 0.12);
    m_batteryVoltage += (target + m_batteryNoise - m_batteryVoltage) * 0.18;
    m_batteryVoltage = qBound(10.5, m_batteryVoltage, 14.8);

    ++m_batterySampleTick;
    if (m_batterySampleTick >= 10) {
        m_batterySampleTick = 0;
        pushBatterySample(m_batteryVoltage);
        persist();
    }
}

void VehicleState::syncWeather()
{
    if (!m_weather || m_weather->condition().isEmpty())
        return;
    if (m_outsideTemp == m_weather->temperature())
        return;
    m_outsideTemp = m_weather->temperature();
    emit changed();
}

QVector<VehicleState::AlertItem> VehicleState::activeAlerts() const
{
    const QString crit = QStringLiteral("#FF3B30");
    const QString warn = QStringLiteral("#FF9500");
    const QString info = QStringLiteral("#007AFF");
    QVector<AlertItem> a;
    auto add = [&a](const QString &text, const QString &color) {
        a.append({text, color});
    };

    if (m_engineFault)
        add(QStringLiteral("发动机故障"), crit);
    if (m_coolant > 105)
        add(QStringLiteral("水温过高"), crit);
    if (m_oilPressureLow)
        add(QStringLiteral("机油压力低"), crit);
    if (m_absFault)
        add(QStringLiteral("ABS故障"), crit);
    if (m_espFault)
        add(QStringLiteral("ESP故障"), crit);
    if (m_speed > 120)
        add(QStringLiteral("超速"), crit);
    if (!m_seatbeltOn && m_speed > 0)
        add(QStringLiteral("安全带未系"), crit);
    if (m_handbrakeOn && (m_gear == QStringLiteral("D") || m_gear == QStringLiteral("R")))
        add(QStringLiteral("手刹未松"), crit);
    if (m_doorAjar && m_speed > 0)
        add(QStringLiteral("车门未关"), crit);
    if (m_hoodOpen && m_speed > 0)
        add(QStringLiteral("引擎盖未关"), crit);
    if (m_trunkOpen && m_speed > 0)
        add(QStringLiteral("后备箱未关"), crit);

    if (tireAlert())
        add(QStringLiteral("胎压偏低"), warn);
    if (m_fuel < 20)
        add(QStringLiteral("油量偏低"), warn);
    if (m_rangeKm < 50)
        add(QStringLiteral("续航不足"), warn);
    if (m_brakeWear)
        add(QStringLiteral("刹车片磨损"), warn);
    if (batteryLow())
        add(QStringLiteral("蓄电池电压低"), warn);
    if (m_coolantLow)
        add(QStringLiteral("冷却液不足"), warn);
    if (m_doorAjar && m_speed <= 0)
        add(QStringLiteral("车门未关"), warn);
    if (m_hoodOpen && m_speed <= 0)
        add(QStringLiteral("引擎盖未关"), warn);
    if (m_trunkOpen && m_speed <= 0)
        add(QStringLiteral("后备箱未关"), warn);
    if (!m_locked && m_gear == QStringLiteral("P"))
        add(QStringLiteral("车门未锁"), warn);

    if (m_washerLow)
        add(QStringLiteral("雨刷水不足"), info);
    if (m_lights && m_gear == QStringLiteral("P"))
        add(QStringLiteral("大灯未关"), info);
    if (serviceDue())
        add(QStringLiteral("保养到期"), info);
    if (m_gear == QStringLiteral("R"))
        add(QStringLiteral("倒车中"), info);
    return a;
}

void VehicleState::advanceAlert()
{
    const int n = activeAlerts().size();
    if (n <= 1)
        return;
    m_alertIndex = (m_alertIndex + 1) % n;
    emit changed();
}

void VehicleState::tick()
{
    bool dirty = false;
    if (m_gear == QStringLiteral("D")) {
        m_speed += m_dir;
        if (m_speed >= 125 || m_speed <= 22)
            m_dir = -m_dir;
        dirty = true;
    } else if (m_gear == QStringLiteral("R") && m_speed != 6) {
        m_speed = 6;
        dirty = true;
    } else if ((m_gear == QStringLiteral("P") || m_gear == QStringLiteral("N")) && m_speed != 0) {
        m_speed = 0;
        dirty = true;
    }

    if (m_speed > 0) {
        m_distanceAcc += m_speed / 3600.0;
        if (m_distanceAcc >= 0.1) {
            const int add = int(m_distanceAcc * 10);
            m_distanceAcc -= add / 10.0;
            m_odometer += add;
            m_trip += add;
            if (m_fuel > 5 && (m_odometer % 3) == 0) {
                --m_fuel;
                m_rangeKm = qMax(20, qRound(m_fuel * 6.1));
            }
            persist();
            dirty = true;
        }
    }

    if (m_coolant < 108 && m_speed > 80) {
        ++m_coolant;
        dirty = true;
    } else if (m_coolant < 92 && m_speed > 30) {
        ++m_coolant;
        dirty = true;
    } else if (m_coolant > 88 && m_speed < 10) {
        --m_coolant;
        dirty = true;
    }

    sampleBattery();
    dirty = true;

    const int n = activeAlerts().size();
    if (n > 0 && m_alertIndex >= n) {
        m_alertIndex = 0;
        dirty = true;
    }

    if (dirty)
        emit changed();
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

bool VehicleState::tireAlert() const
{
    return m_tireFl < 2.3 || m_tireFr < 2.3 || m_tireRl < 2.3 || m_tireRr < 2.3;
}

bool VehicleState::seatbeltOn() const { return m_seatbeltOn; }
bool VehicleState::handbrakeOn() const { return m_handbrakeOn; }
bool VehicleState::oilPressureLow() const { return m_oilPressureLow; }
qreal VehicleState::batteryVoltage() const { return m_batteryVoltage; }
bool VehicleState::batteryLow() const { return m_batteryVoltage < 12.0; }

QVariantList VehicleState::batteryHistory() const
{
    QVariantList out;
    out.reserve(m_batteryHistory.size());
    for (qreal v : m_batteryHistory)
        out.append(v);
    return out;
}

qreal VehicleState::batteryHistoryMin() const { return m_batteryHistoryMin; }
qreal VehicleState::batteryHistoryMax() const { return m_batteryHistoryMax; }
bool VehicleState::brakeWear() const { return m_brakeWear; }
bool VehicleState::engineFault() const { return m_engineFault; }
bool VehicleState::absFault() const { return m_absFault; }
bool VehicleState::espFault() const { return m_espFault; }
bool VehicleState::coolantLow() const { return m_coolantLow; }
bool VehicleState::washerLow() const { return m_washerLow; }
bool VehicleState::doorAjar() const { return m_doorAjar; }
bool VehicleState::hoodOpen() const { return m_hoodOpen; }
bool VehicleState::trunkOpen() const { return m_trunkOpen; }

bool VehicleState::serviceDue() const
{
    return (m_odometer % 5000) > 4950 || (m_odometer % 5000) < 50;
}

QString VehicleState::alertText() const
{
    const auto a = activeAlerts();
    if (a.isEmpty())
        return {};
    return a.at(m_alertIndex % a.size()).text;
}

QString VehicleState::alertColor() const
{
    const auto a = activeAlerts();
    if (a.isEmpty())
        return QStringLiteral("#FF9500");
    return a.at(m_alertIndex % a.size()).color;
}

int VehicleState::alertCount() const
{
    return activeAlerts().size();
}

int VehicleState::alertIndex() const
{
    const int n = activeAlerts().size();
    if (n <= 0)
        return 0;
    return m_alertIndex % n;
}

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
    m_alertIndex = 0;
    emit changed();
}

void VehicleState::toggleLocked()
{
    m_locked = !m_locked;
    persist();
    emit changed();
}

void VehicleState::toggleLights()
{
    m_lights = !m_lights;
    persist();
    emit changed();
}

void VehicleState::toggleSeatbelt()
{
    m_seatbeltOn = !m_seatbeltOn;
    persist();
    emit changed();
}

void VehicleState::toggleHandbrake()
{
    m_handbrakeOn = !m_handbrakeOn;
    persist();
    emit changed();
}

void VehicleState::toggleDoorAjar()
{
    m_doorAjar = !m_doorAjar;
    persist();
    emit changed();
}

void VehicleState::cycleHoodTrunk()
{
    if (!m_hoodOpen && !m_trunkOpen)
        m_hoodOpen = true;
    else if (m_hoodOpen) {
        m_hoodOpen = false;
        m_trunkOpen = true;
    } else
        m_trunkOpen = false;
    persist();
    emit changed();
}

void VehicleState::resetTrip()
{
    m_trip = 0;
    persist();
    emit changed();
}

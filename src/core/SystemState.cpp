#include "SystemState.hpp"
#include "UpdateService.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QProcess>
#include <QSettings>
#include <QTime>
#include <QtConcurrent>
#include <QtMath>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wlanapi.h>
#endif

#if defined(Q_OS_LINUX)
namespace {

void applyLinuxBacklight(qreal brightness)
{
    const QDir dir(QStringLiteral("/sys/class/backlight"));
    const QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (entries.isEmpty())
        return;
    const QString base = dir.filePath(entries.first());
    QFile maxFile(base + QStringLiteral("/max_brightness"));
    if (!maxFile.open(QIODevice::ReadOnly))
        return;
    bool ok = false;
    const int maxV = QString::fromUtf8(maxFile.readAll()).trimmed().toInt(&ok);
    if (!ok || maxV <= 0)
        return;
    const int value = qBound(1, qRound(brightness * maxV), maxV);
    QFile brightFile(base + QStringLiteral("/brightness"));
    if (!brightFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    brightFile.write(QByteArray::number(value));
}

struct LinuxLinkSnapshot {
    int battery = -1;
    QString wifiName;
    int wifiSignal = 0;
};

LinuxLinkSnapshot queryLinuxLink(bool wifiOn)
{
    LinuxLinkSnapshot out;
    if (!wifiOn)
        return out;
    QProcess iw;
    iw.start(QStringLiteral("iwgetid"), {QStringLiteral("-r")});
    if (iw.waitForFinished(800) && iw.exitStatus() == QProcess::NormalExit && iw.exitCode() == 0)
        out.wifiName = QString::fromUtf8(iw.readAllStandardOutput()).trimmed();
    QFile wireless(QStringLiteral("/proc/net/wireless"));
    if (wireless.open(QIODevice::ReadOnly)) {
        const QList<QByteArray> lines = wireless.readAll().split('\n');
        for (const QByteArray &line : lines) {
            if (!line.contains(':') || line.startsWith("Inter") || line.startsWith(" face"))
                continue;
            const QByteArrayList cols = line.simplified().split(' ');
            if (cols.size() < 4)
                continue;
            QByteArray qualityToken = cols.at(2);
            if (qualityToken.endsWith('.'))
                qualityToken.chop(1);
            bool ok = false;
            const double quality = qualityToken.toDouble(&ok);
            if (!ok)
                continue;
            if (quality >= 55)
                out.wifiSignal = 4;
            else if (quality >= 40)
                out.wifiSignal = 3;
            else if (quality >= 25)
                out.wifiSignal = 2;
            else if (quality > 0)
                out.wifiSignal = 1;
            else
                out.wifiSignal = out.wifiName.isEmpty() ? 0 : 1;
            break;
        }
    }
    if (!out.wifiName.isEmpty() && out.wifiSignal == 0)
        out.wifiSignal = 3;
    return out;
}

}
#endif

SystemState::SystemState(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    m_autoTheme = settings.value(QStringLiteral("autoTheme"), true).toBool();
    m_manualDark = settings.value(QStringLiteral("manualDark"), false).toBool();
    m_brightness = qBound(0.15, settings.value(QStringLiteral("brightness"), 0.85).toReal(), 1.0);
    m_volume = qBound(0.0, settings.value(QStringLiteral("volume"), 0.5).toReal(), 1.0);
    m_bluetooth = settings.value(QStringLiteral("bluetooth"), true).toBool();
    m_wifi = settings.value(QStringLiteral("wifi"), true).toBool();
    m_developerMode = settings.value(QStringLiteral("developerMode"), false).toBool();
    m_lockTimeout = settings.value(QStringLiteral("lockTimeout"), 5).toInt();
    if (m_lockTimeout < 0)
        m_lockTimeout = 0;
    m_dark = m_autoTheme ? nightNow() : m_manualDark;
    m_time = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &SystemState::updateTime);
    m_timer.start();
    m_linkTimer.setInterval(20000);
    connect(&m_linkTimer, &QTimer::timeout, this, &SystemState::refreshLink);
    m_linkTimer.start();
    QTimer::singleShot(12000, this, &SystemState::refreshLink);
#if defined(Q_OS_LINUX)
    applyLinuxBacklight(m_brightness);
#endif
}

qreal SystemState::brightness() const
{
    return m_brightness;
}

void SystemState::setBrightness(qreal value)
{
    value = qBound(0.15, value, 1.0);
    if (qFuzzyCompare(m_brightness, value))
        return;
    m_brightness = value;
    QSettings().setValue(QStringLiteral("brightness"), m_brightness);
#if defined(Q_OS_LINUX)
    applyLinuxBacklight(m_brightness);
#endif
    emit brightnessChanged();
}

qreal SystemState::volume() const
{
    return m_volume;
}

void SystemState::setVolume(qreal value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_volume, value))
        return;
    m_volume = value;
    QSettings().setValue(QStringLiteral("volume"), m_volume);
    emit volumeChanged();
}

bool SystemState::bluetooth() const
{
    return m_bluetooth;
}

void SystemState::setBluetooth(bool value)
{
    if (m_bluetooth == value)
        return;
    m_bluetooth = value;
    QSettings().setValue(QStringLiteral("bluetooth"), m_bluetooth);
    emit bluetoothChanged();
}

bool SystemState::wifi() const
{
    return m_wifi;
}

QString SystemState::wifiName() const
{
    return m_wifiName;
}

int SystemState::wifiSignal() const
{
    return m_wifiSignal;
}

int SystemState::battery() const
{
    return m_battery;
}

void SystemState::setWifi(bool value)
{
    if (m_wifi == value)
        return;
    m_wifi = value;
    QSettings().setValue(QStringLiteral("wifi"), m_wifi);
    emit wifiChanged();
    refreshLink();
}

bool SystemState::developerMode() const
{
    return m_developerMode;
}

void SystemState::setDeveloperMode(bool value)
{
    if (m_developerMode == value)
        return;
    m_developerMode = value;
    QSettings().setValue(QStringLiteral("developerMode"), value);
    emit developerModeChanged();
}

int SystemState::lockTimeout() const
{
    return m_lockTimeout;
}

void SystemState::setLockTimeout(int minutes)
{
    if (minutes < 0)
        minutes = 0;
    if (m_lockTimeout == minutes)
        return;
    m_lockTimeout = minutes;
    QSettings().setValue(QStringLiteral("lockTimeout"), m_lockTimeout);
    emit lockTimeoutChanged();
}

QString SystemState::appVersion() const
{
    return UpdateService::readInstalledVersion();
}

void SystemState::unlockDeveloper()
{
    ++m_devTaps;
    if (m_devTaps < 7)
        return;
    m_devTaps = 0;
    setDeveloperMode(true);
}

QVariant SystemState::pref(const QString &key, const QVariant &fallback) const
{
    return QSettings().value(key, fallback);
}

void SystemState::setPref(const QString &key, const QVariant &value)
{
    QSettings().setValue(key, value);
}

QString SystemState::time() const
{
    return m_time;
}

void SystemState::updateTime()
{
    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    if (now != m_time) {
        m_time = now;
        emit timeChanged();
    }
    refreshDark();
}

#ifdef Q_OS_WIN
namespace {

struct WinLinkSnapshot {
    int battery = -1;
    QString wifiName;
    int wifiSignal = 0;
};

WinLinkSnapshot queryWinLink(bool wifiOn)
{
    WinLinkSnapshot out;
    SYSTEM_POWER_STATUS power{};
    if (GetSystemPowerStatus(&power) && power.BatteryLifePercent <= 100)
        out.battery = int(power.BatteryLifePercent);
    if (!wifiOn)
        return out;

    HANDLE wlan = nullptr;
    DWORD version = 0;
    if (WlanOpenHandle(2, nullptr, &version, &wlan) != ERROR_SUCCESS)
        return out;

    PWLAN_INTERFACE_INFO_LIST list = nullptr;
    if (WlanEnumInterfaces(wlan, nullptr, &list) == ERROR_SUCCESS && list) {
        for (DWORD i = 0; i < list->dwNumberOfItems; ++i) {
            PWLAN_CONNECTION_ATTRIBUTES attr = nullptr;
            DWORD size = 0;
            WLAN_OPCODE_VALUE_TYPE type;
            if (WlanQueryInterface(wlan, &list->InterfaceInfo[i].InterfaceGuid,
                    wlan_intf_opcode_current_connection, nullptr, &size,
                    reinterpret_cast<PVOID *>(&attr), &type) != ERROR_SUCCESS || !attr)
                continue;
            if (attr->isState == wlan_interface_state_connected) {
                const DOT11_SSID &ssid = attr->wlanAssociationAttributes.dot11Ssid;
                out.wifiName = QString::fromUtf8(reinterpret_cast<const char *>(ssid.ucSSID), int(ssid.uSSIDLength));
                const ULONG q = attr->wlanAssociationAttributes.wlanSignalQuality;
                if (q >= 76)
                    out.wifiSignal = 4;
                else if (q >= 51)
                    out.wifiSignal = 3;
                else if (q >= 26)
                    out.wifiSignal = 2;
                else if (q > 0)
                    out.wifiSignal = 1;
            }
            WlanFreeMemory(attr);
            if (!out.wifiName.isEmpty())
                break;
        }
        WlanFreeMemory(list);
    }
    WlanCloseHandle(wlan, nullptr);
    return out;
}

}
#endif

void SystemState::applyLinkResults(int battery, const QString &name, int signal)
{
    m_linkBusy = false;
    if (name != m_wifiName) {
        m_wifiName = name;
        emit wifiNameChanged();
    }
    if (signal != m_wifiSignal) {
        m_wifiSignal = signal;
        emit wifiSignalChanged();
    }
    if (battery != m_battery) {
        m_battery = battery;
        emit batteryChanged();
    }
}

void SystemState::refreshLink()
{
#ifdef Q_OS_WIN
    if (m_linkBusy.exchange(true))
        return;
    const bool wifiOn = m_wifi;
    (void)QtConcurrent::run([this, wifiOn]() {
        const WinLinkSnapshot snap = queryWinLink(wifiOn);
        QMetaObject::invokeMethod(this, "applyLinkResults", Qt::QueuedConnection,
                                Q_ARG(int, snap.battery), Q_ARG(QString, snap.wifiName),
                                Q_ARG(int, snap.wifiSignal));
    });
#elif defined(Q_OS_LINUX)
    if (m_linkBusy.exchange(true))
        return;
    const bool wifiOn = m_wifi;
    (void)QtConcurrent::run([this, wifiOn]() {
        const LinuxLinkSnapshot snap = queryLinuxLink(wifiOn);
        QMetaObject::invokeMethod(this, "applyLinkResults", Qt::QueuedConnection,
                                  Q_ARG(int, snap.battery), Q_ARG(QString, snap.wifiName),
                                  Q_ARG(int, snap.wifiSignal));
    });
#else
    const int signal = m_wifi ? 4 : 0;
    if (signal != m_wifiSignal) {
        m_wifiSignal = signal;
        emit wifiSignalChanged();
    }
#endif
}

bool SystemState::autoTheme() const
{
    return m_autoTheme;
}

void SystemState::setAutoTheme(bool value)
{
    if (m_autoTheme == value)
        return;
    m_autoTheme = value;
    QSettings().setValue(QStringLiteral("autoTheme"), value);
    emit autoThemeChanged();
    refreshDark();
}

bool SystemState::manualDark() const
{
    return m_manualDark;
}

void SystemState::setManualDark(bool value)
{
    if (m_manualDark == value)
        return;
    m_manualDark = value;
    QSettings().setValue(QStringLiteral("manualDark"), value);
    emit manualDarkChanged();
    refreshDark();
}

bool SystemState::dark() const
{
    return m_dark;
}

QString SystemState::page() const
{
    return m_dark ? QStringLiteral("#000000") : QStringLiteral("#F2F2F7");
}

QString SystemState::card() const
{
    return m_dark ? QStringLiteral("#CC2C2C2E") : QStringLiteral("#E6FFFFFF");
}

QString SystemState::ink() const
{
    return m_dark ? QStringLiteral("#FFFFFF") : QStringLiteral("#000000");
}

QString SystemState::secondary() const
{
    return m_dark ? QStringLiteral("#98989D") : QStringLiteral("#8E8E93");
}

QString SystemState::fill() const
{
    return m_dark ? QStringLiteral("#3D3A3A3C") : QStringLiteral("#33767680");
}

QString SystemState::highlight() const
{
    return selected();
}

QString SystemState::tint() const
{
    return m_dark ? QStringLiteral("#0A84FF") : QStringLiteral("#007AFF");
}

QString SystemState::separator() const
{
    return m_dark ? QStringLiteral("#40FFFFFF") : QStringLiteral("#29000000");
}

QString SystemState::selected() const
{
    return m_dark ? QStringLiteral("#330A84FF") : QStringLiteral("#1A007AFF");
}

QString SystemState::elevated() const
{
    return m_dark ? QStringLiteral("#E63A3A3C") : QStringLiteral("#F2FFFFFF");
}

QString SystemState::danger() const
{
    return QStringLiteral("#FF3B30");
}

QString SystemState::success() const
{
    return QStringLiteral("#34C759");
}

QString SystemState::warning() const
{
    return QStringLiteral("#FF9500");
}

bool SystemState::nightNow() const
{
    const int hour = QTime::currentTime().hour();
    return hour < 6 || hour >= 18;
}

void SystemState::refreshDark()
{
    const bool next = m_autoTheme ? nightNow() : m_manualDark;
    if (m_dark == next)
        return;
    m_dark = next;
    emit darkChanged();
}

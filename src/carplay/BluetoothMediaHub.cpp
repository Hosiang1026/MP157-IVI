#include "BluetoothMediaHub.hpp"

#include "BluetoothDevices.hpp"

#include <QProcess>
#include <QSettings>

namespace {

QString normalizeMac(QString value)
{
    value = value.trimmed().toUpper().replace(QLatin1Char('-'), QLatin1Char(':'));
    return value;
}

} // namespace

BluetoothMediaHub::BluetoothMediaHub(QObject *parent)
    : QObject(parent)
    , m_status(QStringLiteral("未选择"))
{
    loadSettings();
    m_timer.setInterval(4000);
    connect(&m_timer, &QTimer::timeout, this, &BluetoothMediaHub::refresh);
    m_timer.start();
    refresh();
}

QVariantList BluetoothMediaHub::devices() const { return m_devices; }
QString BluetoothMediaHub::activeAddress() const { return m_activeAddress; }
QString BluetoothMediaHub::activeName() const { return m_activeName; }
QString BluetoothMediaHub::status() const { return m_status; }

bool BluetoothMediaHub::activeConnected() const
{
    for (const QVariant &item : m_devices) {
        const QVariantMap map = item.toMap();
        if (normalizeMac(map.value(QStringLiteral("address")).toString()) == m_activeAddress)
            return map.value(QStringLiteral("connected")).toBool();
    }
    return false;
}

void BluetoothMediaHub::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void BluetoothMediaHub::loadSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("btMedia"));
    m_activeAddress = normalizeMac(settings.value(QStringLiteral("activeAddress")).toString());
    m_activeName = settings.value(QStringLiteral("activeName")).toString();
    settings.endGroup();
}

void BluetoothMediaHub::saveSettings() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("btMedia"));
    settings.setValue(QStringLiteral("activeAddress"), m_activeAddress);
    settings.setValue(QStringLiteral("activeName"), m_activeName);
    settings.endGroup();
}

void BluetoothMediaHub::routePulseBluez(const QString &address) const
{
#ifdef Q_OS_LINUX
    const QString compact = QString(address).remove(QLatin1Char(':')).toUpper();
    QProcess::execute(QStringLiteral("pactl"),
                      {QStringLiteral("set-card-profile"),
                       QStringLiteral("bluez_card.%1").arg(QString(address).replace(QLatin1Char(':'), QLatin1Char('_'))),
                       QStringLiteral("a2dp_sink")});
    Q_UNUSED(compact);
#else
    Q_UNUSED(address);
#endif
}

void BluetoothMediaHub::refresh()
{
    QVariantList listed = BluetoothDevices::listDevices();
    QVariantList out;
    for (const QVariant &item : listed) {
        QVariantMap map = item.toMap();
        const QString addr = normalizeMac(map.value(QStringLiteral("address")).toString());
        map.insert(QStringLiteral("address"), addr);
        map.insert(QStringLiteral("active"), addr == m_activeAddress);
        out.append(map);
        if (addr == m_activeAddress) {
            const QString name = map.value(QStringLiteral("name")).toString();
            if (!name.isEmpty() && name != m_activeName) {
                m_activeName = name;
                emit activeChanged();
            }
        }
    }
    m_devices = out;
    emit devicesChanged();
    emit activeChanged();

    if (m_activeAddress.isEmpty())
        setStatus(QStringLiteral("未选择蓝牙音源"));
    else if (activeConnected())
        setStatus(QStringLiteral("正在使用 %1").arg(m_activeName.isEmpty() ? m_activeAddress : m_activeName));
    else
        setStatus(QStringLiteral("已选 %1 · 未连接").arg(m_activeName.isEmpty() ? m_activeAddress : m_activeName));
}

bool BluetoothMediaHub::selectDevice(const QString &address)
{
    const QString mac = normalizeMac(address);
    if (mac.size() != 17) {
        setStatus(QStringLiteral("地址无效"));
        return false;
    }

    QString name = mac;
    for (const QVariant &item : m_devices) {
        const QVariantMap map = item.toMap();
        if (normalizeMac(map.value(QStringLiteral("address")).toString()) == mac) {
            name = map.value(QStringLiteral("name")).toString();
            if (name.isEmpty())
                name = mac;
            break;
        }
    }

    setStatus(QStringLiteral("切换到 %1…").arg(name));

    // Keep ACL links; only move A2DP audio to the selected phone.
    for (const QVariant &item : m_devices) {
        const QVariantMap map = item.toMap();
        const QString other = normalizeMac(map.value(QStringLiteral("address")).toString());
        if (other.size() != 17 || other == mac)
            continue;
        BluetoothDevices::disconnectAudioProfile(other, nullptr);
    }

    QString error;
    if (!BluetoothDevices::isPaired(mac)) {
        if (!BluetoothDevices::authenticate(mac, &error)) {
            setStatus(error.isEmpty() ? QStringLiteral("配对失败") : error);
            return false;
        }
    }
    if (!BluetoothDevices::connectDevice(mac, &error)) {
        setStatus(error.isEmpty() ? QStringLiteral("连接失败") : error);
        refresh();
        return false;
    }
    BluetoothDevices::connectAudioProfile(mac, nullptr);

    routePulseBluez(mac);
    m_activeAddress = mac;
    m_activeName = name;
    saveSettings();
    emit activeChanged();
    refresh();
    setStatus(QStringLiteral("正在使用 %1").arg(m_activeName));
    return true;
}

void BluetoothMediaHub::clearActive()
{
    if (!m_activeAddress.isEmpty())
        BluetoothDevices::disconnectDevice(m_activeAddress, nullptr);
    m_activeAddress.clear();
    m_activeName.clear();
    saveSettings();
    emit activeChanged();
    refresh();
    setStatus(QStringLiteral("已切回本机"));
}

bool BluetoothMediaHub::pairDevice(const QString &address)
{
    QString error;
    if (!BluetoothDevices::authenticate(address, &error)) {
        setStatus(error.isEmpty() ? QStringLiteral("配对失败") : error);
        refresh();
        return false;
    }
    setStatus(QStringLiteral("配对成功"));
    refresh();
    return true;
}

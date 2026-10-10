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
QString BluetoothMediaHub::phoneAddress() const { return m_phoneAddress; }
QString BluetoothMediaHub::phoneName() const { return m_phoneName; }
QString BluetoothMediaHub::status() const { return m_status; }

bool BluetoothMediaHub::deviceConnected(const QString &address) const
{
    const QString mac = normalizeMac(address);
    for (const QVariant &item : m_devices) {
        const QVariantMap map = item.toMap();
        if (normalizeMac(map.value(QStringLiteral("address")).toString()) == mac)
            return map.value(QStringLiteral("connected")).toBool();
    }
    return false;
}

QString BluetoothMediaHub::deviceName(const QString &address) const
{
    const QString mac = normalizeMac(address);
    for (const QVariant &item : m_devices) {
        const QVariantMap map = item.toMap();
        if (normalizeMac(map.value(QStringLiteral("address")).toString()) == mac) {
            const QString name = map.value(QStringLiteral("name")).toString();
            return name.isEmpty() ? mac : name;
        }
    }
    return mac;
}

bool BluetoothMediaHub::activeConnected() const
{
    return deviceConnected(m_activeAddress);
}

bool BluetoothMediaHub::phoneConnected() const
{
    return deviceConnected(m_phoneAddress);
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
    m_phoneAddress = normalizeMac(settings.value(QStringLiteral("phoneAddress")).toString());
    m_phoneName = settings.value(QStringLiteral("phoneName")).toString();
    settings.endGroup();
}

void BluetoothMediaHub::saveSettings() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("btMedia"));
    settings.setValue(QStringLiteral("activeAddress"), m_activeAddress);
    settings.setValue(QStringLiteral("activeName"), m_activeName);
    settings.setValue(QStringLiteral("phoneAddress"), m_phoneAddress);
    settings.setValue(QStringLiteral("phoneName"), m_phoneName);
    settings.endGroup();
}

void BluetoothMediaHub::routePulseBluez(const QString &address) const
{
#ifdef Q_OS_LINUX
    QProcess::execute(QStringLiteral("pactl"),
                      {QStringLiteral("set-card-profile"),
                       QStringLiteral("bluez_card.%1").arg(QString(address).replace(QLatin1Char(':'), QLatin1Char('_'))),
                       QStringLiteral("a2dp_sink")});
#else
    Q_UNUSED(address);
#endif
}

void BluetoothMediaHub::refresh()
{
    QVariantList listed = BluetoothDevices::listDevices();

    bool phoneOk = false;
    QString autoAddr;
    QString autoName;
    for (const QVariant &item : listed) {
        const QVariantMap map = item.toMap();
        const QString addr = normalizeMac(map.value(QStringLiteral("address")).toString());
        if (addr.size() != 17)
            continue;
        const bool paired = map.value(QStringLiteral("paired")).toBool();
        const bool connected = map.value(QStringLiteral("connected")).toBool();
        if (addr == m_phoneAddress && connected)
            phoneOk = true;
        if (autoAddr.isEmpty() && paired && connected) {
            autoAddr = addr;
            autoName = map.value(QStringLiteral("name")).toString();
        }
    }
    bool phoneDirty = false;
    if (!phoneOk && !autoAddr.isEmpty()) {
        if (m_phoneAddress != autoAddr) {
            m_phoneAddress = autoAddr;
            m_phoneName = autoName.isEmpty() ? autoAddr : autoName;
            phoneDirty = true;
            BluetoothDevices::connectMessageProfile(m_phoneAddress, nullptr);
        } else if (!autoName.isEmpty() && autoName != m_phoneName) {
            m_phoneName = autoName;
            phoneDirty = true;
        }
        if (phoneDirty)
            saveSettings();
    }

    QVariantList out;
    for (const QVariant &item : listed) {
        QVariantMap map = item.toMap();
        const QString addr = normalizeMac(map.value(QStringLiteral("address")).toString());
        map.insert(QStringLiteral("address"), addr);
        map.insert(QStringLiteral("active"), addr == m_activeAddress);
        map.insert(QStringLiteral("phone"), addr == m_phoneAddress);
        out.append(map);
        if (addr == m_activeAddress) {
            const QString name = map.value(QStringLiteral("name")).toString();
            if (!name.isEmpty() && name != m_activeName) {
                m_activeName = name;
                emit activeChanged();
            }
        }
        if (addr == m_phoneAddress) {
            const QString name = map.value(QStringLiteral("name")).toString();
            if (!name.isEmpty() && name != m_phoneName) {
                m_phoneName = name;
                phoneDirty = true;
            }
        }
    }
    m_devices = out;
    emit devicesChanged();
    emit activeChanged();
    emit phoneChanged();

    QStringList parts;
    if (!m_phoneAddress.isEmpty()) {
        parts << QStringLiteral("电话 %1%2")
                     .arg(m_phoneName.isEmpty() ? m_phoneAddress : m_phoneName,
                          phoneConnected() ? QString() : QStringLiteral("·未连"));
    }
    if (m_activeAddress.isEmpty()) {
        parts << QStringLiteral("媒体 本机");
    } else {
        parts << QStringLiteral("媒体 %1%2")
                     .arg(m_activeName.isEmpty() ? m_activeAddress : m_activeName,
                          activeConnected() ? QString() : QStringLiteral("·未连"));
    }
    setStatus(parts.join(QStringLiteral(" · ")));
}

bool BluetoothMediaHub::selectDevice(const QString &address)
{
    const QString mac = normalizeMac(address);
    if (mac.size() != 17) {
        setStatus(QStringLiteral("地址无效"));
        return false;
    }

    const QString name = deviceName(mac);
    setStatus(QStringLiteral("切换媒体到 %1…").arg(name));

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
    return true;
}

void BluetoothMediaHub::clearActive()
{
    if (!m_activeAddress.isEmpty())
        BluetoothDevices::disconnectAudioProfile(m_activeAddress, nullptr);
    m_activeAddress.clear();
    m_activeName.clear();
    saveSettings();
    emit activeChanged();
    refresh();
}

bool BluetoothMediaHub::selectPhoneDevice(const QString &address)
{
    const QString mac = normalizeMac(address);
    if (mac.size() != 17) {
        setStatus(QStringLiteral("地址无效"));
        return false;
    }

    const QString name = deviceName(mac);
    setStatus(QStringLiteral("切换电话到 %1…").arg(name));

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
    BluetoothDevices::connectMessageProfile(mac, nullptr);

    m_phoneAddress = mac;
    m_phoneName = name;
    saveSettings();
    emit phoneChanged();
    refresh();
    return true;
}

void BluetoothMediaHub::clearPhone()
{
    m_phoneAddress.clear();
    m_phoneName.clear();
    saveSettings();
    emit phoneChanged();
    refresh();
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

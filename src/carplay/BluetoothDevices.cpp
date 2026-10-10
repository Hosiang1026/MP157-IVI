#include "BluetoothDevices.hpp"

#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <QVariantMap>
#include <algorithm>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bluetoothapis.h>
#pragma comment(lib, "Bthprops.lib")

namespace {

QString formatAddress(const BLUETOOTH_ADDRESS &addr)
{
    return QStringLiteral("%1:%2:%3:%4:%5:%6")
        .arg(addr.rgBytes[5], 2, 16, QLatin1Char('0'))
        .arg(addr.rgBytes[4], 2, 16, QLatin1Char('0'))
        .arg(addr.rgBytes[3], 2, 16, QLatin1Char('0'))
        .arg(addr.rgBytes[2], 2, 16, QLatin1Char('0'))
        .arg(addr.rgBytes[1], 2, 16, QLatin1Char('0'))
        .arg(addr.rgBytes[0], 2, 16, QLatin1Char('0'))
        .toUpper();
}

bool parseAddress(const QString &address, BLUETOOTH_ADDRESS *out)
{
    const QStringList parts = address.split(QLatin1Char(':'));
    if (parts.size() != 6 || !out)
        return false;
    BLUETOOTH_ADDRESS addr{};
    for (int i = 0; i < 6; ++i) {
        bool ok = false;
        const int v = parts[i].toInt(&ok, 16);
        if (!ok || v < 0 || v > 255)
            return false;
        addr.rgBytes[5 - i] = BYTE(v);
    }
    *out = addr;
    return true;
}

bool lookupDevice(const QString &address, BLUETOOTH_DEVICE_INFO *info)
{
    BLUETOOTH_ADDRESS addr{};
    if (!parseAddress(address, &addr) || !info)
        return false;

    BLUETOOTH_DEVICE_SEARCH_PARAMS params{};
    params.dwSize = sizeof(params);
    params.fReturnAuthenticated = TRUE;
    params.fReturnRemembered = TRUE;
    params.fReturnUnknown = TRUE;
    params.fReturnConnected = TRUE;
    params.fIssueInquiry = FALSE;
    params.cTimeoutMultiplier = 1;
    params.hRadio = nullptr;

    BLUETOOTH_DEVICE_INFO found{};
    found.dwSize = sizeof(found);
    HBLUETOOTH_DEVICE_FIND find = BluetoothFindFirstDevice(&params, &found);
    if (!find)
        return false;

    bool ok = false;
    do {
        if (memcmp(found.Address.rgBytes, addr.rgBytes, 6) == 0) {
            *info = found;
            ok = true;
            break;
        }
    } while (BluetoothFindNextDevice(find, &found));
    BluetoothFindDeviceClose(find);
    return ok;
}

} // namespace
#endif

#ifdef Q_OS_LINUX
namespace {

QString runBt(const QStringList &args, int timeoutMs = 8000)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(QStringLiteral("bluetoothctl"), args);
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(1000);
        return {};
    }
    return QString::fromUtf8(proc.readAll());
}

QString normalizeMac(QString value)
{
    value = value.trimmed().toUpper().replace(QLatin1Char('-'), QLatin1Char(':'));
    return value;
}

} // namespace
#endif

namespace BluetoothDevices {

QString localAdapterAddress()
{
#ifdef Q_OS_WIN
    BLUETOOTH_FIND_RADIO_PARAMS params{};
    params.dwSize = sizeof(params);
    HANDLE radio = nullptr;
    const HBLUETOOTH_RADIO_FIND find = BluetoothFindFirstRadio(&params, &radio);
    if (!find)
        return {};
    QString address;
    do {
        BLUETOOTH_RADIO_INFO info{};
        info.dwSize = sizeof(info);
        if (BluetoothGetRadioInfo(radio, &info) == ERROR_SUCCESS) {
            address = formatAddress(info.address);
            ::CloseHandle(radio);
            break;
        }
        ::CloseHandle(radio);
        radio = nullptr;
    } while (BluetoothFindNextRadio(find, &radio));
    BluetoothFindRadioClose(find);
    return address;
#elif defined(Q_OS_LINUX)
    const QString out = runBt({QStringLiteral("show")});
    const QRegularExpression re(QStringLiteral("Controller\\s+([0-9A-Fa-f:]{17})"));
    const auto m = re.match(out);
    return m.hasMatch() ? normalizeMac(m.captured(1)) : QString();
#else
    return {};
#endif
}

QVariantList listDevices()
{
    QVariantList out;
#ifdef Q_OS_WIN
    BLUETOOTH_DEVICE_SEARCH_PARAMS params{};
    params.dwSize = sizeof(params);
    params.fReturnAuthenticated = TRUE;
    params.fReturnRemembered = TRUE;
    params.fReturnUnknown = TRUE;
    params.fReturnConnected = TRUE;
    params.fIssueInquiry = FALSE;
    params.cTimeoutMultiplier = 1;
    params.hRadio = nullptr;

    BLUETOOTH_DEVICE_INFO info{};
    info.dwSize = sizeof(info);

    HBLUETOOTH_DEVICE_FIND find = BluetoothFindFirstDevice(&params, &info);
    if (!find)
        return out;

    QSet<QString> seen;
    do {
        const QString address = formatAddress(info.Address);
        if (seen.contains(address))
            continue;
        seen.insert(address);

        QVariantMap item;
        item.insert(QStringLiteral("name"),
                    QString::fromWCharArray(info.szName).trimmed());
        item.insert(QStringLiteral("address"), address);
        item.insert(QStringLiteral("paired"), info.fAuthenticated != FALSE);
        item.insert(QStringLiteral("connected"), info.fConnected != FALSE);
        out.append(item);
    } while (BluetoothFindNextDevice(find, &info));

    BluetoothFindDeviceClose(find);

    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        const auto ma = a.toMap();
        const auto mb = b.toMap();
        if (ma.value(QStringLiteral("paired")).toBool() != mb.value(QStringLiteral("paired")).toBool())
            return ma.value(QStringLiteral("paired")).toBool();
        if (ma.value(QStringLiteral("connected")).toBool() != mb.value(QStringLiteral("connected")).toBool())
            return ma.value(QStringLiteral("connected")).toBool();
        return ma.value(QStringLiteral("name")).toString() < mb.value(QStringLiteral("name")).toString();
    });
#elif defined(Q_OS_LINUX)
    runBt({QStringLiteral("power"), QStringLiteral("on")});

    QSet<QString> paired;
    for (const QString &line : runBt({QStringLiteral("devices"), QStringLiteral("Paired")})
                                   .split(QLatin1Char('\n'))) {
        const QRegularExpression re(QStringLiteral("Device\\s+([0-9A-Fa-f:]{17})"));
        const auto m = re.match(line);
        if (m.hasMatch())
            paired.insert(normalizeMac(m.captured(1)));
    }

    QSet<QString> seen;
    for (const QString &line : runBt({QStringLiteral("devices")}).split(QLatin1Char('\n'))) {
        const QRegularExpression re(QStringLiteral("Device\\s+([0-9A-Fa-f:]{17})\\s+(.*)"));
        const auto m = re.match(line);
        if (!m.hasMatch())
            continue;
        const QString address = normalizeMac(m.captured(1));
        if (seen.contains(address))
            continue;
        seen.insert(address);
        const QString info = runBt({QStringLiteral("info"), address});
        QVariantMap item;
        item.insert(QStringLiteral("name"), m.captured(2).trimmed());
        item.insert(QStringLiteral("address"), address);
        item.insert(QStringLiteral("paired"), paired.contains(address)
                                                  || info.contains(QStringLiteral("Paired: yes")));
        item.insert(QStringLiteral("connected"), info.contains(QStringLiteral("Connected: yes")));
        out.append(item);
    }
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        const auto ma = a.toMap();
        const auto mb = b.toMap();
        if (ma.value(QStringLiteral("paired")).toBool() != mb.value(QStringLiteral("paired")).toBool())
            return ma.value(QStringLiteral("paired")).toBool();
        if (ma.value(QStringLiteral("connected")).toBool() != mb.value(QStringLiteral("connected")).toBool())
            return ma.value(QStringLiteral("connected")).toBool();
        return ma.value(QStringLiteral("name")).toString() < mb.value(QStringLiteral("name")).toString();
    });
#endif
    return out;
}

bool isPaired(const QString &address)
{
#ifdef Q_OS_WIN
    BLUETOOTH_DEVICE_INFO info{};
    info.dwSize = sizeof(info);
    if (!lookupDevice(address, &info))
        return false;
    return info.fAuthenticated != FALSE;
#elif defined(Q_OS_LINUX)
    const QString mac = normalizeMac(address);
    if (mac.size() != 17)
        return false;
    const QString info = runBt({QStringLiteral("info"), mac});
    return info.contains(QStringLiteral("Paired: yes"));
#else
    Q_UNUSED(address);
    return false;
#endif
}

bool authenticate(const QString &address, QString *error)
{
#ifdef Q_OS_WIN
    BLUETOOTH_DEVICE_INFO info{};
    info.dwSize = sizeof(info);
    if (!lookupDevice(address, &info)) {
        BLUETOOTH_ADDRESS addr{};
        if (!parseAddress(address, &addr)) {
            if (error)
                *error = QStringLiteral("蓝牙地址无效");
            return false;
        }
        info.Address = addr;
    }

    if (info.fAuthenticated) {
        if (error)
            *error = QStringLiteral("已配对");
        return true;
    }

    const DWORD rc = BluetoothAuthenticateDevice(nullptr, nullptr, &info, nullptr, TRUE);
    if (rc == ERROR_CANCELLED) {
        if (error)
            *error = QStringLiteral("已取消，请在电脑和手机上都点配对");
        return false;
    }
    if (rc != ERROR_SUCCESS) {
        if (error)
            *error = QStringLiteral("配对失败 (%1)，请确认手机上的配对码").arg(rc);
        return false;
    }

    for (int i = 0; i < 20; ++i) {
        Sleep(200);
        if (isPaired(address))
            return true;
    }

    if (error)
        *error = QStringLiteral("手机未确认配对码，配对未完成");
    return false;
#elif defined(Q_OS_LINUX)
    const QString mac = normalizeMac(address);
    if (mac.size() != 17) {
        if (error)
            *error = QStringLiteral("蓝牙地址无效");
        return false;
    }
    if (isPaired(mac))
        return true;
    runBt({QStringLiteral("power"), QStringLiteral("on")});
    runBt({QStringLiteral("agent"), QStringLiteral("NoInputNoOutput")});
    runBt({QStringLiteral("default-agent")});
    runBt({QStringLiteral("pairable"), QStringLiteral("on")});
    const QString out = runBt({QStringLiteral("pair"), mac}, 45000);
    runBt({QStringLiteral("trust"), mac});
    for (int i = 0; i < 25; ++i) {
        if (isPaired(mac))
            return true;
        QThread::msleep(200);
    }
    if (error)
        *error = out.trimmed().isEmpty() ? QStringLiteral("配对失败，请在手机上确认")
                                         : out.trimmed();
    return false;
#else
    Q_UNUSED(address);
    if (error)
        *error = QStringLiteral("当前平台不支持蓝牙配对");
    return false;
#endif
}

bool connectDevice(const QString &address, QString *error)
{
#ifdef Q_OS_WIN
    BLUETOOTH_DEVICE_INFO info{};
    info.dwSize = sizeof(info);
    if (!lookupDevice(address, &info)) {
        if (error)
            *error = QStringLiteral("未找到设备");
        return false;
    }
    if (!info.fAuthenticated) {
        if (!authenticate(address, error))
            return false;
        if (!lookupDevice(address, &info)) {
            if (error)
                *error = QStringLiteral("配对后未找到设备");
            return false;
        }
    }
    // A2DP Sink UUID on host side: phone streams audio to us.
    GUID a2dp = {0x0000110b, 0x0000, 0x1000, {0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb}};
    GUID avrcp = {0x0000110e, 0x0000, 0x1000, {0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb}};
    BluetoothSetServiceState(nullptr, &info, &a2dp, BLUETOOTH_SERVICE_ENABLE);
    BluetoothSetServiceState(nullptr, &info, &avrcp, BLUETOOTH_SERVICE_ENABLE);
    return true;
#elif defined(Q_OS_LINUX)
    const QString mac = normalizeMac(address);
    if (mac.size() != 17) {
        if (error)
            *error = QStringLiteral("蓝牙地址无效");
        return false;
    }
    runBt({QStringLiteral("power"), QStringLiteral("on")});
    const QString out = runBt({QStringLiteral("connect"), mac}, 30000);
    for (int i = 0; i < 20; ++i) {
        const QString info = runBt({QStringLiteral("info"), mac});
        if (info.contains(QStringLiteral("Connected: yes")))
            return true;
        QThread::msleep(250);
    }
    if (error)
        *error = out.trimmed().isEmpty() ? QStringLiteral("连接失败") : out.trimmed();
    return false;
#else
    Q_UNUSED(address);
    if (error)
        *error = QStringLiteral("当前平台不支持");
    return false;
#endif
}

bool disconnectDevice(const QString &address, QString *error)
{
#ifdef Q_OS_WIN
    BLUETOOTH_DEVICE_INFO info{};
    info.dwSize = sizeof(info);
    if (!lookupDevice(address, &info)) {
        if (error)
            *error = QStringLiteral("未找到设备");
        return false;
    }
    GUID a2dp = {0x0000110b, 0x0000, 0x1000, {0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb}};
    GUID avrcp = {0x0000110e, 0x0000, 0x1000, {0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb}};
    BluetoothSetServiceState(nullptr, &info, &a2dp, BLUETOOTH_SERVICE_DISABLE);
    BluetoothSetServiceState(nullptr, &info, &avrcp, BLUETOOTH_SERVICE_DISABLE);
    return true;
#elif defined(Q_OS_LINUX)
    const QString mac = normalizeMac(address);
    if (mac.size() != 17) {
        if (error)
            *error = QStringLiteral("蓝牙地址无效");
        return false;
    }
    runBt({QStringLiteral("disconnect"), mac}, 15000);
    return true;
#else
    Q_UNUSED(address);
    if (error)
        *error = QStringLiteral("当前平台不支持");
    return false;
#endif
}

bool setAudioEnabled(const QString &address, bool enabled, QString *error)
{
    return enabled ? connectAudioProfile(address, error) : disconnectAudioProfile(address, error);
}

bool connectAudioProfile(const QString &address, QString *error)
{
#ifdef Q_OS_WIN
    return connectDevice(address, error);
#elif defined(Q_OS_LINUX)
    const QString mac = normalizeMac(address);
    if (mac.size() != 17) {
        if (error)
            *error = QStringLiteral("蓝牙地址无效");
        return false;
    }
    // Ensure base ACL link, then A2DP Sink profile only.
    runBt({QStringLiteral("power"), QStringLiteral("on")});
    runBt({QStringLiteral("connect"), mac}, 20000);
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(QStringLiteral("bluetoothctl"),
               {QStringLiteral("connect-profile"), mac,
                QStringLiteral("0000110b-0000-1000-8000-00805f9b34fb")});
    if (!proc.waitForFinished(20000))
        proc.kill();
    // Fallback: some stacks use menu syntax / already connected with A2DP.
    Q_UNUSED(error);
    return true;
#else
    Q_UNUSED(address);
    if (error)
        *error = QStringLiteral("当前平台不支持");
    return false;
#endif
}

bool disconnectAudioProfile(const QString &address, QString *error)
{
#ifdef Q_OS_WIN
    return disconnectDevice(address, error);
#elif defined(Q_OS_LINUX)
    const QString mac = normalizeMac(address);
    if (mac.size() != 17) {
        if (error)
            *error = QStringLiteral("蓝牙地址无效");
        return false;
    }
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(QStringLiteral("bluetoothctl"),
               {QStringLiteral("disconnect-profile"), mac,
                QStringLiteral("0000110b-0000-1000-8000-00805f9b34fb")});
    if (!proc.waitForFinished(15000))
        proc.kill();
    Q_UNUSED(error);
    return true;
#else
    Q_UNUSED(address);
    if (error)
        *error = QStringLiteral("当前平台不支持");
    return false;
#endif
}

bool connectMessageProfile(const QString &address, QString *error)
{
#ifdef Q_OS_WIN
    BLUETOOTH_DEVICE_INFO info{};
    info.dwSize = sizeof(info);
    if (!lookupDevice(address, &info)) {
        if (error)
            *error = QStringLiteral("未找到设备");
        return false;
    }
    if (!info.fAuthenticated) {
        if (!authenticate(address, error))
            return false;
        if (!lookupDevice(address, &info)) {
            if (error)
                *error = QStringLiteral("配对后未找到设备");
            return false;
        }
    }
    GUID map = {0x00001132, 0x0000, 0x1000, {0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb}};
    GUID hfp = {0x0000111f, 0x0000, 0x1000, {0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb}};
    BluetoothSetServiceState(nullptr, &info, &map, BLUETOOTH_SERVICE_ENABLE);
    BluetoothSetServiceState(nullptr, &info, &hfp, BLUETOOTH_SERVICE_ENABLE);
    return true;
#elif defined(Q_OS_LINUX)
    const QString mac = normalizeMac(address);
    if (mac.size() != 17) {
        if (error)
            *error = QStringLiteral("蓝牙地址无效");
        return false;
    }
    runBt({QStringLiteral("power"), QStringLiteral("on")});
    runBt({QStringLiteral("connect"), mac}, 20000);
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(QStringLiteral("bluetoothctl"),
               {QStringLiteral("connect-profile"), mac,
                QStringLiteral("00001132-0000-1000-8000-00805f9b34fb")});
    if (!proc.waitForFinished(20000))
        proc.kill();
    Q_UNUSED(error);
    return true;
#else
    Q_UNUSED(address);
    if (error)
        *error = QStringLiteral("当前平台不支持");
    return false;
#endif
}

} // namespace BluetoothDevices

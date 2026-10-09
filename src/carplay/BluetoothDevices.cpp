#include "BluetoothDevices.hpp"

#include <QSet>
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
    params.fIssueInquiry = TRUE;
    params.cTimeoutMultiplier = 2;
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
#else
    Q_UNUSED(address);
    if (error)
        *error = QStringLiteral("当前平台不支持蓝牙配对");
    return false;
#endif
}

} // namespace BluetoothDevices

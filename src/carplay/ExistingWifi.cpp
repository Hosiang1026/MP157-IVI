#include "ExistingWifi.hpp"

#include <QHostAddress>
#include <QNetworkInterface>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QVariantMap>
#include <algorithm>
#include <cstring>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wlanapi.h>
#pragma comment(lib, "wlanapi.lib")
#endif

namespace {

bool isLinkLocal(const QHostAddress &ip)
{
    return (ip.toIPv4Address() & 0xFFFF0000u) == 0xA9FE0000u;
}

bool isVirtualName(const QString &name)
{
    const QString n = name.toLower();
    return n.contains(QStringLiteral("virtual")) ||
           n.contains(QStringLiteral("vmware")) ||
           n.contains(QStringLiteral("vbox")) ||
           n.contains(QStringLiteral("hyper-v")) ||
           n.contains(QStringLiteral("docker")) ||
           n.contains(QStringLiteral("wsl")) ||
           n.contains(QStringLiteral("zerotier")) ||
           n.contains(QStringLiteral("vpn")) ||
           n.contains(QStringLiteral("tap")) ||
           n.contains(QStringLiteral("tun")) ||
           n.contains(QStringLiteral("vethernet"));
}

QString ipv4OnInterface(const QNetworkInterface &iface)
{
    for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
        const QHostAddress ip = entry.ip();
        if (ip.protocol() != QAbstractSocket::IPv4Protocol)
            continue;
        if (ip.isLoopback() || ip.isNull() || isLinkLocal(ip))
            continue;
        return ip.toString();
    }
    return {};
}

bool isWlanName(const QString &name)
{
    const QString n = name.toLower();
    return n.contains(QStringLiteral("wlan")) ||
           n.contains(QStringLiteral("wi-fi")) ||
           n.contains(QStringLiteral("wifi")) ||
           n.contains(QStringLiteral("无线"));
}

int channelFromFrequency(ULONG freq)
{
    if (freq == 0)
        return 0;
    // WLAN API documents kHz; some stacks still return MHz.
    const int mhz = freq < 10000 ? int(freq) : int(freq / 1000);
    if (mhz >= 2412 && mhz <= 2484) {
        if (mhz == 2484)
            return 14;
        return (mhz - 2407) / 5;
    }
    if (mhz >= 5000 && mhz < 5900)
        return (mhz - 5000) / 5;
    if (mhz >= 5955 && mhz <= 7115)
        return (mhz - 5950) / 5;
    return 0;
}

QString netshWlanInterfacesText()
{
    QProcess proc;
    proc.setProgram(QStringLiteral("cmd.exe"));
    proc.setArguments({QStringLiteral("/c"),
                       QStringLiteral("chcp 437>nul & netsh wlan show interfaces")});
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start();
    if (!proc.waitForFinished(5000)) {
        proc.kill();
        return {};
    }
    const QByteArray raw = proc.readAll();
    QString out = QString::fromLatin1(raw);
    if (!out.contains(QStringLiteral("Channel"), Qt::CaseInsensitive)
        && !out.contains(QStringLiteral("SSID")))
        out = QString::fromLocal8Bit(raw);
    return out;
}

int channelFromNetsh()
{
    const QString out = netshWlanInterfacesText();
    const QStringList lines = out.split(QRegularExpression(QStringLiteral("[\\r\\n]+")));
    for (QString line : lines) {
        line = line.trimmed();
        if (!line.contains(QStringLiteral("Channel"), Qt::CaseInsensitive)
            && !line.contains(QStringLiteral("通道")))
            continue;
        if (line.contains(QStringLiteral("BSSID"), Qt::CaseInsensitive))
            continue;
        const QRegularExpression re(QStringLiteral("(\\d+)\\s*$"));
        const auto m = re.match(line);
        if (m.hasMatch()) {
            const int ch = m.captured(1).toInt();
            if (ch > 0 && ch < 300)
                return ch;
        }
    }
    return 0;
}

QByteArray bssidFromNetsh()
{
    const QString out = netshWlanInterfacesText();
    const QRegularExpression re(QStringLiteral("(?:AP BSSID|BSSID)\\s*:\\s*([0-9a-fA-F:-]{17})"),
                                QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(out);
    if (!m.hasMatch())
        return {};
    const QStringList parts = m.captured(1).split(QRegularExpression(QStringLiteral("[:-]")));
    if (parts.size() != 6)
        return {};
    QByteArray bssid(6, 0);
    for (int i = 0; i < 6; ++i) {
        bool ok = false;
        bssid[i] = char(parts[i].toInt(&ok, 16));
        if (!ok)
            return {};
    }
    return bssid;
}

} // namespace

QString ExistingWifi::interfaceMac()
{
    const QString wantIp = primaryIpv4();
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            (iface.flags() & QNetworkInterface::IsLoopBack) ||
            isVirtualName(iface.humanReadableName()))
            continue;
        const QString ip = ipv4OnInterface(iface);
        if (ip.isEmpty())
            continue;
        if (!wantIp.isEmpty() && ip != wantIp)
            continue;
        const QString mac = iface.hardwareAddress().trimmed();
        if (mac.size() >= 11)
            return mac;
    }
    return {};
}

QString ExistingWifi::primaryIpv4()
{
    QString fallback;
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            !(iface.flags() & QNetworkInterface::IsRunning) ||
            (iface.flags() & QNetworkInterface::IsLoopBack) ||
            isVirtualName(iface.humanReadableName()))
            continue;
        const QString ip = ipv4OnInterface(iface);
        if (ip.isEmpty())
            continue;
        if (isWlanName(iface.humanReadableName()) || isWlanName(iface.name()))
            return ip;
        if (fallback.isEmpty())
            fallback = ip;
    }
    return fallback;
}

QString ExistingWifi::currentSsid()
{
#ifdef Q_OS_WIN
    HANDLE wlan = nullptr;
    DWORD version = 0;
    if (WlanOpenHandle(2, nullptr, &version, &wlan) != ERROR_SUCCESS)
        return {};

    QString ssid;
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
                const DOT11_SSID &dot = attr->wlanAssociationAttributes.dot11Ssid;
                ssid = QString::fromUtf8(reinterpret_cast<const char *>(dot.ucSSID),
                                         int(dot.uSSIDLength));
            }
            WlanFreeMemory(attr);
            if (!ssid.isEmpty())
                break;
        }
        WlanFreeMemory(list);
    }
    WlanCloseHandle(wlan, nullptr);
    return ssid;
#else
    return {};
#endif
}

bool ExistingWifi::isConnected()
{
    return !currentSsid().isEmpty() && !primaryIpv4().isEmpty();
}

QString ExistingWifi::profilePassword(const QString &ssid)
{
    if (ssid.isEmpty())
        return {};
    QProcess proc;
    proc.setProgram(QStringLiteral("cmd.exe"));
    proc.setArguments({QStringLiteral("/c"),
                       QStringLiteral("chcp 437>nul & netsh wlan show profile name=\"%1\" key=clear")
                           .arg(ssid)});
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start();
    if (!proc.waitForFinished(5000)) {
        proc.kill();
        return {};
    }
    const QByteArray raw = proc.readAll();
    QString out = QString::fromLatin1(raw);
    if (!out.contains(QStringLiteral("Key Content"), Qt::CaseInsensitive)
        && !out.contains(QStringLiteral("关键内容")))
        out = QString::fromLocal8Bit(raw);
    const QRegularExpression re(
        QStringLiteral("(?:Key Content|关键内容)\\s*:\\s*(.+)\\s*$"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
    const auto m = re.match(out);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

ExistingWifi ExistingWifi::query()
{
    ExistingWifi info;
    info.ipv4 = primaryIpv4();
    info.mac = interfaceMac();
#ifdef Q_OS_WIN
    HANDLE wlan = nullptr;
    DWORD version = 0;
    if (WlanOpenHandle(2, nullptr, &version, &wlan) == ERROR_SUCCESS) {
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
                    const DOT11_SSID &dot = attr->wlanAssociationAttributes.dot11Ssid;
                    info.ssid = QString::fromUtf8(reinterpret_cast<const char *>(dot.ucSSID),
                                                  int(dot.uSSIDLength));
                    info.bssid = QByteArray(reinterpret_cast<const char *>(
                                                attr->wlanAssociationAttributes.dot11Bssid),
                                            6);
                    WlanScan(wlan, &list->InterfaceInfo[i].InterfaceGuid,
                             &attr->wlanAssociationAttributes.dot11Ssid, nullptr, nullptr);
                    Sleep(400);
                    PWLAN_BSS_LIST bss = nullptr;
                    if (WlanGetNetworkBssList(wlan, &list->InterfaceInfo[i].InterfaceGuid,
                                              &attr->wlanAssociationAttributes.dot11Ssid,
                                              attr->wlanAssociationAttributes.dot11BssType,
                                              FALSE, nullptr, &bss) == ERROR_SUCCESS && bss) {
                        for (DWORD b = 0; b < bss->dwNumberOfItems; ++b) {
                            if (memcmp(bss->wlanBssEntries[b].dot11Bssid,
                                       attr->wlanAssociationAttributes.dot11Bssid, 6) != 0)
                                continue;
                            info.channel = channelFromFrequency(
                                bss->wlanBssEntries[b].ulChCenterFrequency);
                            break;
                        }
                        if (info.channel == 0 && bss->dwNumberOfItems > 0) {
                            info.channel = channelFromFrequency(
                                bss->wlanBssEntries[0].ulChCenterFrequency);
                        }
                        WlanFreeMemory(bss);
                    }
                }
                WlanFreeMemory(attr);
                if (!info.ssid.isEmpty())
                    break;
            }
            WlanFreeMemory(list);
        }
        WlanCloseHandle(wlan, nullptr);
    }
#else
    info.ssid = currentSsid();
#endif
    if (info.ssid.isEmpty())
        info.ssid = currentSsid();
    if (info.channel <= 0)
        info.channel = channelFromNetsh();
    if (info.bssid.size() != 6)
        info.bssid = bssidFromNetsh();
    return info;
}

QVariantList ExistingWifi::scanNetworks()
{
    QVariantList out;
#ifdef Q_OS_WIN
    HANDLE wlan = nullptr;
    DWORD version = 0;
    if (WlanOpenHandle(2, nullptr, &version, &wlan) != ERROR_SUCCESS)
        return out;

    PWLAN_INTERFACE_INFO_LIST ifaces = nullptr;
    if (WlanEnumInterfaces(wlan, nullptr, &ifaces) != ERROR_SUCCESS || !ifaces) {
        WlanCloseHandle(wlan, nullptr);
        return out;
    }

    QSet<QString> seen;
    for (DWORD i = 0; i < ifaces->dwNumberOfItems; ++i) {
        const GUID &guid = ifaces->InterfaceInfo[i].InterfaceGuid;
        WlanScan(wlan, &guid, nullptr, nullptr, nullptr);

        PWLAN_AVAILABLE_NETWORK_LIST nets = nullptr;
        if (WlanGetAvailableNetworkList(wlan, &guid, 0, nullptr, &nets) != ERROR_SUCCESS || !nets)
            continue;

        for (DWORD n = 0; n < nets->dwNumberOfItems; ++n) {
            const WLAN_AVAILABLE_NETWORK &net = nets->Network[n];
            const QString ssid = QString::fromUtf8(reinterpret_cast<const char *>(net.dot11Ssid.ucSSID),
                                                   int(net.dot11Ssid.uSSIDLength));
            if (ssid.isEmpty() || seen.contains(ssid))
                continue;
            seen.insert(ssid);

            QVariantMap item;
            item.insert(QStringLiteral("ssid"), ssid);
            item.insert(QStringLiteral("signal"), int(net.wlanSignalQuality));
            item.insert(QStringLiteral("secured"),
                        net.bSecurityEnabled != FALSE);
            item.insert(QStringLiteral("connected"),
                        (net.dwFlags & WLAN_AVAILABLE_NETWORK_CONNECTED) != 0);
            out.append(item);
        }
        WlanFreeMemory(nets);
    }
    WlanFreeMemory(ifaces);
    WlanCloseHandle(wlan, nullptr);

    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        const auto ma = a.toMap();
        const auto mb = b.toMap();
        if (ma.value(QStringLiteral("connected")).toBool() != mb.value(QStringLiteral("connected")).toBool())
            return ma.value(QStringLiteral("connected")).toBool();
        return ma.value(QStringLiteral("signal")).toInt() > mb.value(QStringLiteral("signal")).toInt();
    });
#endif
    return out;
}

bool ExistingWifi::connectTo(const QString &ssid, const QString &password, QString *error)
{
#ifdef Q_OS_WIN
    if (ssid.isEmpty()) {
        if (error)
            *error = QStringLiteral("SSID 为空");
        return false;
    }

    HANDLE wlan = nullptr;
    DWORD version = 0;
    if (WlanOpenHandle(2, nullptr, &version, &wlan) != ERROR_SUCCESS) {
        if (error)
            *error = QStringLiteral("打开 WLAN 失败");
        return false;
    }

    PWLAN_INTERFACE_INFO_LIST ifaces = nullptr;
    if (WlanEnumInterfaces(wlan, nullptr, &ifaces) != ERROR_SUCCESS || !ifaces || ifaces->dwNumberOfItems == 0) {
        if (error)
            *error = QStringLiteral("无无线网卡");
        if (ifaces)
            WlanFreeMemory(ifaces);
        WlanCloseHandle(wlan, nullptr);
        return false;
    }

    const GUID guid = ifaces->InterfaceInfo[0].InterfaceGuid;
    WlanFreeMemory(ifaces);

    const QString escapedSsid = QString(ssid).toHtmlEscaped();
    const QString escapedPass = QString(password).toHtmlEscaped();
    QString profileXml;
    if (password.isEmpty()) {
        profileXml = QStringLiteral(
                         "<?xml version=\"1.0\"?>"
                         "<WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">"
                         "<name>%1</name><SSIDConfig><SSID><name>%1</name></SSID></SSIDConfig>"
                         "<connectionType>ESS</connectionType><connectionMode>auto</connectionMode>"
                         "<MSM><security><authEncryption>"
                         "<authentication>open</authentication><encryption>none</encryption>"
                         "<useOneX>false</useOneX></authEncryption></security></MSM></WLANProfile>")
                         .arg(escapedSsid);
    } else {
        profileXml = QStringLiteral(
                         "<?xml version=\"1.0\"?>"
                         "<WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">"
                         "<name>%1</name><SSIDConfig><SSID><name>%1</name></SSID></SSIDConfig>"
                         "<connectionType>ESS</connectionType><connectionMode>auto</connectionMode>"
                         "<MSM><security><authEncryption>"
                         "<authentication>WPA2PSK</authentication><encryption>AES</encryption>"
                         "<useOneX>false</useOneX></authEncryption>"
                         "<sharedKey><keyType>passPhrase</keyType><protected>false</protected>"
                         "<keyMaterial>%2</keyMaterial></sharedKey></security></MSM></WLANProfile>")
                         .arg(escapedSsid, escapedPass);
    }

    const std::wstring xml = profileXml.toStdWString();
    DWORD reason = 0;
    const DWORD setRc = WlanSetProfile(wlan, &guid, 0, xml.c_str(), nullptr, TRUE, nullptr, &reason);
    if (setRc != ERROR_SUCCESS) {
        if (error)
            *error = QStringLiteral("写入配置失败 (%1)").arg(setRc);
        WlanCloseHandle(wlan, nullptr);
        return false;
    }

    DOT11_SSID dot{};
    const QByteArray ssidUtf8 = ssid.toUtf8();
    dot.uSSIDLength = ULONG(qMin(ssidUtf8.size(), 32));
    memcpy(dot.ucSSID, ssidUtf8.constData(), dot.uSSIDLength);

    WLAN_CONNECTION_PARAMETERS params{};
    params.wlanConnectionMode = wlan_connection_mode_profile;
    const std::wstring profileName = ssid.toStdWString();
    params.strProfile = profileName.c_str();
    params.pDot11Ssid = &dot;
    params.dot11BssType = dot11_BSS_type_infrastructure;

    const DWORD connRc = WlanConnect(wlan, &guid, &params, nullptr);
    WlanCloseHandle(wlan, nullptr);
    if (connRc != ERROR_SUCCESS) {
        if (error)
            *error = QStringLiteral("连接失败 (%1)").arg(connRc);
        return false;
    }
    return true;
#else
    Q_UNUSED(ssid);
    Q_UNUSED(password);
    if (error)
        *error = QStringLiteral("当前平台不支持 Wi‑Fi 连接");
    return false;
#endif
}

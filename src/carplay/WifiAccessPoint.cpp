#include "WifiAccessPoint.hpp"

#include <QDir>
#include <QFile>
#include <QNetworkInterface>
#include <QStandardPaths>
#include <QThread>

namespace {

QByteArray macToBytes(const QString &mac)
{
    const QStringList parts = mac.split(QLatin1Char(':'));
    if (parts.size() != 6)
        return {};
    QByteArray out(6, 0);
    for (int i = 0; i < 6; ++i) {
        bool ok = false;
        out[i] = char(parts[i].toInt(&ok, 16));
        if (!ok)
            return {};
    }
    return out;
}

} // namespace

WifiAccessPoint &WifiAccessPoint::instance()
{
    static WifiAccessPoint ap;
    return ap;
}

bool WifiAccessPoint::isRunning() const
{
#ifdef Q_OS_LINUX
    return m_running && m_hostapd.state() == QProcess::Running;
#else
    return false;
#endif
}

WifiAccessPointInfo WifiAccessPoint::info() const
{
    return m_info;
}

bool WifiAccessPoint::run(const QString &program, const QStringList &args, QString *output,
                          int timeoutMs) const
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(program, args);
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(1000);
        if (output)
            *output = QStringLiteral("timeout");
        return false;
    }
    if (output)
        *output = QString::fromUtf8(proc.readAll());
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

QString WifiAccessPoint::pickIface() const
{
#ifdef Q_OS_LINUX
    const QString env = qEnvironmentVariable("IVI_WIFI_IFACE").trimmed();
    if (!env.isEmpty())
        return env;

    QString out;
    if (run(QStringLiteral("iw"), {QStringLiteral("dev")}, &out)) {
        QString current;
        for (QString line : out.split(QLatin1Char('\n'))) {
            line = line.trimmed();
            if (line.startsWith(QStringLiteral("Interface "))) {
                current = line.mid(10).trimmed();
                continue;
            }
            if (!current.isEmpty() && line.contains(QStringLiteral("type"))) {
                if (!line.contains(QStringLiteral("p2p"))
                    && (current.startsWith(QStringLiteral("wlan"))
                        || current.startsWith(QStringLiteral("wlp"))))
                    return current;
                current.clear();
            }
        }
    }

    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        const QString name = iface.name();
        if (name.startsWith(QStringLiteral("wlan")) || name.startsWith(QStringLiteral("wlp")))
            return name;
    }
#endif
    return {};
}

bool WifiAccessPoint::writeConfigs(const QString &iface, const QString &ssid, const QString &password,
                                   int channel, QString *error)
{
    m_confDir = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                    .filePath(QStringLiteral("ivi-carplay-ap"));
    QDir().mkpath(m_confDir);

    const QString hostapdPath = QDir(m_confDir).filePath(QStringLiteral("hostapd.conf"));
    const QString dnsmasqPath = QDir(m_confDir).filePath(QStringLiteral("dnsmasq.conf"));
    const QString hostapdConf = QStringLiteral(
                                    "interface=%1\n"
                                    "driver=nl80211\n"
                                    "ssid=%2\n"
                                    "hw_mode=g\n"
                                    "channel=%3\n"
                                    "ieee80211n=1\n"
                                    "wmm_enabled=1\n"
                                    "auth_algs=1\n"
                                    "wpa=2\n"
                                    "wpa_passphrase=%4\n"
                                    "wpa_key_mgmt=WPA-PSK\n"
                                    "rsn_pairwise=CCMP\n")
                                    .arg(iface, ssid)
                                    .arg(channel)
                                    .arg(password);
    const QString dnsmasqConf = QStringLiteral(
                                    "interface=%1\n"
                                    "bind-interfaces\n"
                                    "dhcp-range=192.168.50.10,192.168.50.100,12h\n"
                                    "dhcp-option=3,192.168.50.1\n"
                                    "dhcp-option=6,192.168.50.1\n")
                                    .arg(iface);

    QFile ha(hostapdPath);
    if (!ha.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("无法写 hostapd.conf");
        return false;
    }
    ha.write(hostapdConf.toUtf8());
    ha.close();

    QFile dn(dnsmasqPath);
    if (!dn.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("无法写 dnsmasq.conf");
        return false;
    }
    dn.write(dnsmasqConf.toUtf8());
    dn.close();
    return true;
}

bool WifiAccessPoint::start(const QString &ssid, const QString &password, int channel, QString *error)
{
#ifndef Q_OS_LINUX
    Q_UNUSED(ssid);
    Q_UNUSED(password);
    Q_UNUSED(channel);
    if (error)
        *error = QStringLiteral("仅 Linux 支持自建热点");
    return false;
#else
    if (ssid.isEmpty() || password.size() < 8) {
        if (error)
            *error = QStringLiteral("SSID 为空或密码短于 8 位");
        return false;
    }
    if (isRunning())
        stop();

    const QString iface = pickIface();
    if (iface.isEmpty()) {
        if (error)
            *error = QStringLiteral("未找到 wlan 网卡");
        return false;
    }

    const int ch = channel > 0 ? channel : 6;
    if (!writeConfigs(iface, ssid, password, ch, error))
        return false;

    QString out;
    run(QStringLiteral("rfkill"), {QStringLiteral("unblock"), QStringLiteral("wifi")}, &out);
    run(QStringLiteral("nmcli"),
        {QStringLiteral("dev"), QStringLiteral("set"), iface, QStringLiteral("managed"),
         QStringLiteral("no")},
        &out);
    run(QStringLiteral("ip"),
        {QStringLiteral("link"), QStringLiteral("set"), iface, QStringLiteral("down")}, &out);
    run(QStringLiteral("ip"),
        {QStringLiteral("addr"), QStringLiteral("flush"), QStringLiteral("dev"), iface}, &out);
    run(QStringLiteral("ip"),
        {QStringLiteral("link"), QStringLiteral("set"), iface, QStringLiteral("up")}, &out);
    if (!run(QStringLiteral("ip"),
             {QStringLiteral("addr"), QStringLiteral("add"), QStringLiteral("192.168.50.1/24"),
              QStringLiteral("dev"), iface},
             &out)) {
        if (!out.contains(QStringLiteral("File exists"))) {
            if (error)
                *error = QStringLiteral("配置 IP 失败: %1").arg(out.trimmed());
            return false;
        }
    }

    const QString hostapdConf = QDir(m_confDir).filePath(QStringLiteral("hostapd.conf"));
    const QString dnsmasqConf = QDir(m_confDir).filePath(QStringLiteral("dnsmasq.conf"));

    m_hostapd.setProgram(QStringLiteral("hostapd"));
    m_hostapd.setArguments({hostapdConf});
    m_hostapd.setProcessChannelMode(QProcess::MergedChannels);
    m_hostapd.start();
    if (!m_hostapd.waitForStarted(3000)) {
        if (error)
            *error = QStringLiteral("hostapd 启动失败（请安装 hostapd）");
        return false;
    }
    QThread::msleep(800);
    if (m_hostapd.state() != QProcess::Running) {
        if (error)
            *error = QStringLiteral("hostapd 退出: %1")
                         .arg(QString::fromUtf8(m_hostapd.readAll()).trimmed());
        return false;
    }

    m_dnsmasq.setProgram(QStringLiteral("dnsmasq"));
    m_dnsmasq.setArguments({QStringLiteral("-C"), dnsmasqConf, QStringLiteral("-k")});
    m_dnsmasq.setProcessChannelMode(QProcess::MergedChannels);
    m_dnsmasq.start();
    if (!m_dnsmasq.waitForStarted(3000)) {
        m_hostapd.kill();
        m_hostapd.waitForFinished(1000);
        if (error)
            *error = QStringLiteral("dnsmasq 启动失败（请安装 dnsmasq）");
        return false;
    }

    QString mac;
    for (const QNetworkInterface &ni : QNetworkInterface::allInterfaces()) {
        if (ni.name() == iface) {
            mac = ni.hardwareAddress().trimmed().toUpper();
            break;
        }
    }
    if (mac.isEmpty()) {
        QFile f(QStringLiteral("/sys/class/net/%1/address").arg(iface));
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            mac = QString::fromUtf8(f.readAll()).trimmed().toUpper();
    }

    m_info.iface = iface;
    m_info.ssid = ssid;
    m_info.password = password;
    m_info.ipv4 = QStringLiteral("192.168.50.1");
    m_info.mac = mac;
    m_info.channel = ch;
    m_info.bssid = macToBytes(mac);
    m_running = true;
    return true;
#endif
}

void WifiAccessPoint::stop()
{
#ifdef Q_OS_LINUX
    if (m_dnsmasq.state() != QProcess::NotRunning) {
        m_dnsmasq.terminate();
        if (!m_dnsmasq.waitForFinished(1500))
            m_dnsmasq.kill();
        m_dnsmasq.waitForFinished(1000);
    }
    if (m_hostapd.state() != QProcess::NotRunning) {
        m_hostapd.terminate();
        if (!m_hostapd.waitForFinished(1500))
            m_hostapd.kill();
        m_hostapd.waitForFinished(1000);
    }
    if (!m_info.iface.isEmpty()) {
        QString out;
        run(QStringLiteral("ip"),
            {QStringLiteral("addr"), QStringLiteral("flush"), QStringLiteral("dev"), m_info.iface}, &out);
        run(QStringLiteral("nmcli"),
            {QStringLiteral("dev"), QStringLiteral("set"), m_info.iface, QStringLiteral("managed"),
             QStringLiteral("yes")},
            &out);
    }
#endif
    m_running = false;
    m_info = {};
}

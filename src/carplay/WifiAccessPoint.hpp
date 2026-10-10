#pragma once

#include <QByteArray>
#include <QProcess>
#include <QString>

struct WifiAccessPointInfo {
    QString iface;
    QString ssid;
    QString password;
    QString ipv4;
    QString mac;
    int channel = 0;
    QByteArray bssid;
};

class WifiAccessPoint {
public:
    static WifiAccessPoint &instance();

    bool isRunning() const;
    WifiAccessPointInfo info() const;

    bool start(const QString &ssid, const QString &password, int channel = 6, QString *error = nullptr);
    void stop();

private:
    WifiAccessPoint() = default;

    QString pickIface() const;
    bool writeConfigs(const QString &iface, const QString &ssid, const QString &password, int channel,
                      QString *error);
    bool run(const QString &program, const QStringList &args, QString *output = nullptr,
             int timeoutMs = 8000) const;

    bool m_running = false;
    WifiAccessPointInfo m_info;
    QProcess m_hostapd;
    QProcess m_dnsmasq;
    QString m_confDir;
};

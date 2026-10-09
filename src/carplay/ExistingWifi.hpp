#pragma once

#include <QString>
#include <QVariantList>

struct ExistingWifi {
    QString ssid;
    QString ipv4;
    QString mac;
    int channel = 0;
    QByteArray bssid;

    static ExistingWifi query();
    static QString primaryIpv4();
    static QString currentSsid();
    static QString interfaceMac();
    static QString profilePassword(const QString &ssid);
    static bool isConnected();
    static QVariantList scanNetworks();
    static bool connectTo(const QString &ssid, const QString &password, QString *error = nullptr);
};

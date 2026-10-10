#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace Iap2Protocol {

constexpr quint16 kStartIdentification = 0x1d00;
constexpr quint16 kIdentificationInformation = 0x1d01;
constexpr quint16 kIdentificationAccepted = 0x1d02;
constexpr quint16 kIdentificationRejected = 0x1d03;

constexpr quint16 kRequestCertificate = 0xaa00;
constexpr quint16 kAccessoryCertificate = 0xaa01;
constexpr quint16 kRequestChallenge = 0xaa02;
constexpr quint16 kChallengeResponse = 0xaa03;
constexpr quint16 kAuthenticationFailed = 0xaa04;
constexpr quint16 kAuthenticationSucceeded = 0xaa05;

constexpr quint16 kRequestAccessoryWifiConfiguration = 0x5702;
constexpr quint16 kAccessoryWifiConfiguration = 0x5703;

constexpr quint16 kCarPlayAvailability = 0x4300;
constexpr quint16 kCarPlayStartSession = 0x4301;

constexpr quint16 kWirelessCarPlayUpdate = 0x4e0d;
constexpr quint16 kDeviceTransportIdentifier = 0x4e0e;

constexpr quint16 kStartNowPlayingUpdates = 0x5000;
constexpr quint16 kStartPowerUpdates = 0xae00;
constexpr quint16 kStartCommunicationsUpdates = 0x4157;
constexpr quint16 kStartCallStateUpdates = 0x4154;

struct WirelessIdentity {
    QString name = QStringLiteral("MP157-IVI");
    QString model = QStringLiteral("MP157");
    QString manufacturer = QStringLiteral("MP157");
    QString serial = QStringLiteral("001");
    QString firmwareVersion = QStringLiteral("0.1");
    QString hardwareVersion = QStringLiteral("0.1");
    QString language = QStringLiteral("en");
    QString externalAccessoryProtocol = QStringLiteral("com.mp157.ivi");
    QString bluetoothMac;
    QString ssid;
};

struct WirelessStartSession {
    QString ssid;
    QString passphrase;
    quint8 channel = 0;
    quint8 securityType = 2;
    QStringList ipAddresses;
    quint32 airPlayPort = 7000;
    QString deviceIdentifier;
    QString publicKey;
    QString sourceVersion;
};

QByteArray buildIdentificationWireless(const WirelessIdentity &id);
QByteArray buildAccessoryWifiConfig(const QString &ssid,
                                    const QString &passphrase,
                                    quint8 channel,
                                    quint8 securityType,
                                    const QByteArray &bssid = QByteArray());
QByteArray buildCarPlayStartSessionWireless(const WirelessStartSession &session);
QByteArray buildAuthCertificate(const QByteArray &certificate);
QByteArray buildAuthResponse(const QByteArray &signature);
QByteArray parseChallenge(const QByteArray &csmFrameOrBody);
QList<QByteArray> buildMinimalSubscriptions();

} // namespace Iap2Protocol

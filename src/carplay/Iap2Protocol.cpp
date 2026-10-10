#include "Iap2Protocol.hpp"

#include "Iap2Body.hpp"
#include "Iap2Wire.hpp"

namespace Iap2Protocol {

static QByteArray macBytes(const QString &mac)
{
    const QStringList parts = mac.split(QLatin1Char(':'));
    if (parts.size() != 6)
        return {};
    QByteArray out(6, Qt::Uninitialized);
    for (int i = 0; i < 6; ++i) {
        bool ok = false;
        const int v = parts[i].toInt(&ok, 16);
        if (!ok || v < 0 || v > 255)
            return {};
        out[i] = char(v);
    }
    return out;
}

static QByteArray frame(quint16 messageId, const Iap2Body &body)
{
    return Iap2Wire::encodeFrame(messageId, body.encode());
}

static QList<quint16> wirelessSentMessages()
{
    return {
        0xaa01, 0xaa03,
        0x5000, 0x5002,
        0xae00, 0xae02,
        0x4157, 0x4159,
        0x4154, 0x4156,
        0x4301,
        0x5703,
    };
}

static QList<quint16> wirelessReceivedMessages()
{
    return {
        0xaa00, 0xaa02, 0xaa04, 0xaa05,
        0xea00, 0xea01,
        0x5001,
        0xae01,
        0x4158, 0x4155,
        0x4300,
        0x4e0d, 0x4e0e, 0x5702,
    };
}

QByteArray buildIdentificationWireless(const WirelessIdentity &id)
{
    const QByteArray bt = macBytes(id.bluetoothMac);
    Iap2Body body;
    body.addString(0, id.name)
        .addString(1, id.model)
        .addString(2, id.manufacturer)
        .addString(3, id.serial)
        .addString(4, id.firmwareVersion)
        .addString(5, id.hardwareVersion)
        .addU16List(6, wirelessSentMessages())
        .addU16List(7, wirelessReceivedMessages())
        .addU8(8, 0)
        .addU16(9, 20)
        .addGroup(10, [&](Iap2Body &g) {
            g.addU8(0, 1)
                .addString(1, id.externalAccessoryProtocol)
                .addU8(2, 0);
        })
        .addString(12, id.language)
        .addString(13, id.language)
        .addGroup(17, [&](Iap2Body &g) {
            g.addU16(0, 0)
                .addString(1, QStringLiteral("blue"))
                .addVoid(2)
                .addBytes(3, bt)
                .addString(4, QStringLiteral("blue"))
                .addVoid(5);
        })
        .addGroup(24, [&](Iap2Body &g) {
            g.addU16(0, 1)
                .addString(1, id.ssid)
                .addVoid(2)
                .addU16(3, 1)
                .addVoid(4)
                .addVoid(5);
        });
    return frame(kIdentificationInformation, body);
}

QByteArray buildAccessoryWifiConfig(const QString &ssid,
                                    const QString &passphrase,
                                    quint8 channel,
                                    quint8 securityType,
                                    const QByteArray &bssid)
{
    Iap2Body body;
    if (bssid.size() == 6)
        body.addBytes(0, bssid);
    body.addString(1, ssid)
        .addString(2, passphrase)
        .addU8(3, securityType)
        .addU8(4, channel);
    return frame(kAccessoryWifiConfiguration, body);
}

QByteArray buildCarPlayStartSessionWireless(const WirelessStartSession &session)
{
    Iap2Body body;
    body.addGroup(1, [&](Iap2Body &g) {
            g.addString(0, session.ssid)
                .addString(1, session.passphrase)
                .addU8(2, session.channel);
            for (const QString &ip : session.ipAddresses)
                g.addString(3, ip);
            g.addU8(4, session.securityType);
        })
        .addU32(2, session.airPlayPort);
    if (!session.deviceIdentifier.isEmpty())
        body.addString(3, session.deviceIdentifier);
    body.addString(4, session.publicKey)
        .addString(5, session.sourceVersion);
    return frame(kCarPlayStartSession, body);
}

QByteArray buildAuthCertificate(const QByteArray &certificate)
{
    Iap2Body body;
    body.addBytes(0, certificate);
    return frame(kAccessoryCertificate, body);
}

QByteArray buildAuthResponse(const QByteArray &signature)
{
    Iap2Body body;
    body.addBytes(0, signature);
    return frame(kChallengeResponse, body);
}

QByteArray parseChallenge(const QByteArray &csmFrameOrBody)
{
    QByteArray body = csmFrameOrBody;
    if (body.size() >= Iap2Wire::kCsmHeaderBytes
        && quint8(body[0]) == 0x40 && quint8(body[1]) == 0x40) {
        const quint16 messageId = quint16((quint8(body[4]) << 8) | quint8(body[5]));
        if (messageId != kRequestChallenge)
            return {};
        body = body.mid(Iap2Wire::kCsmHeaderBytes);
    }
    return Iap2Body::firstParam(body, 0);
}

QList<QByteArray> buildMinimalSubscriptions()
{
    QList<QByteArray> out;

    {
        Iap2Body body;
        body.addGroup(0, [](Iap2Body &g) {
                for (quint16 id : {1, 4, 6, 12, 26})
                    g.addVoid(id);
            })
            .addGroup(1, [](Iap2Body &g) {
                for (quint16 id : {0, 1, 7})
                    g.addVoid(id);
            });
        out.append(frame(kStartNowPlayingUpdates, body));
    }
    {
        Iap2Body body;
        body.addVoid(4).addVoid(5).addVoid(6);
        out.append(frame(kStartPowerUpdates, body));
    }
    {
        Iap2Body body;
        body.addVoid(0).addVoid(4).addVoid(5);
        out.append(frame(kStartCommunicationsUpdates, body));
    }
    {
        Iap2Body body;
        for (quint16 id : {0, 1, 2, 3, 4, 11})
            body.addVoid(id);
        out.append(frame(kStartCallStateUpdates, body));
    }
    return out;
}

} // namespace Iap2Protocol

#pragma once

#include <QByteArray>
#include <QString>

class AirPlayIdentity {
public:
    static AirPlayIdentity generate();
    static AirPlayIdentity loadOrCreate(const QString &path = QString());

    QByteArray privateKey() const { return m_seed; }
    QByteArray publicKey() const { return m_publicKey; }
    QString pairingId() const { return m_pairingId; }
    QString publicKeyHex() const;

    bool save(const QString &path = QString()) const;
    static QString defaultPath();

private:
    AirPlayIdentity() = default;
    void derivePublic();

    QByteArray m_seed;
    QByteArray m_publicKey;
    QString m_pairingId;
};

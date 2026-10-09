#include "AirPlayIdentity.hpp"

#include "AirPlayCrypto.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QUuid>

QString AirPlayIdentity::defaultPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("airplay-identity.bin"));
}

void AirPlayIdentity::derivePublic()
{
    m_publicKey.resize(32);
    AirPlayCrypto::ed25519PublicKey(reinterpret_cast<const uint8_t *>(m_seed.constData()),
                                    reinterpret_cast<uint8_t *>(m_publicKey.data()));
}

AirPlayIdentity AirPlayIdentity::generate()
{
    AirPlayIdentity id;
    id.m_seed = AirPlayCrypto::randomBytes(32);
    id.m_pairingId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    id.derivePublic();
    return id;
}

AirPlayIdentity AirPlayIdentity::loadOrCreate(const QString &path)
{
    const QString filePath = path.isEmpty() ? defaultPath() : path;
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray data = file.readAll();
        file.close();
        if (data.size() >= 32 + 36) {
            AirPlayIdentity id;
            id.m_seed = data.left(32);
            id.m_pairingId = QString::fromUtf8(data.mid(32)).trimmed();
            if (id.m_pairingId.size() >= 36 && id.m_seed.size() == 32) {
                id.derivePublic();
                return id;
            }
        }
    }
    AirPlayIdentity id = generate();
    id.save(filePath);
    return id;
}

QString AirPlayIdentity::publicKeyHex() const
{
    return QString::fromLatin1(m_publicKey.toHex());
}

bool AirPlayIdentity::save(const QString &path) const
{
    const QString filePath = path.isEmpty() ? defaultPath() : path;
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    QByteArray blob = m_seed;
    blob.append(m_pairingId.toUtf8());
    return file.write(blob) == blob.size();
}

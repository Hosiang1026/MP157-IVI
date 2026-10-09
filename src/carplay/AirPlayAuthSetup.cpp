#include "AirPlayAuthSetup.hpp"

#include "AirPlayCrypto.hpp"
#include "LocalMfiAuth.hpp"

namespace AirPlayAuthSetup {

static void putU32(QByteArray &out, int value)
{
    out.append(char((value >> 24) & 0xff));
    out.append(char((value >> 16) & 0xff));
    out.append(char((value >> 8) & 0xff));
    out.append(char(value & 0xff));
}

QByteArray handle(const QByteArray &body, LocalMfiAuth &mfi)
{
    if (!mfi.isReady() || body.size() != 33 || quint8(body[0]) != 1)
        return {};
    const QByteArray peerPublic = body.mid(1);
    const auto pair = AirPlayCrypto::x25519Generate();
    const QByteArray shared = AirPlayCrypto::x25519Shared(pair.privateKey, peerPublic);
    if (shared.isEmpty() || shared == QByteArray(32, char(0)))
        return {};

    const QByteArray aesKey = AirPlayCrypto::sha1({QByteArray("AES-KEY"), shared}).left(16);
    const QByteArray aesIv = AirPlayCrypto::sha1({QByteArray("AES-IV"), shared}).left(16);

    QByteArray digest;
    if (mfi.protocolMajor() == 2)
        digest = AirPlayCrypto::sha1({pair.publicKey, peerPublic});
    else
        digest = AirPlayCrypto::sha256({pair.publicKey, peerPublic});

    const QByteArray signature = mfi.signChallenge(digest);
    if (signature.isEmpty())
        return {};
    const QByteArray encryptedSig = AirPlayCrypto::aesCtr128(aesKey, aesIv, signature);
    if (encryptedSig.isEmpty())
        return {};

    const QByteArray cert = mfi.certificate();
    QByteArray response = pair.publicKey;
    putU32(response, cert.size());
    response.append(cert);
    putU32(response, encryptedSig.size());
    response.append(encryptedSig);
    return response;
}

} // namespace AirPlayAuthSetup

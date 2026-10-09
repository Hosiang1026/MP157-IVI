#pragma once

#include <QByteArray>
#include <QList>
#include <cstdint>

namespace AirPlayCrypto {

QByteArray randomBytes(int n);

QByteArray sha512(const QByteArray &a);
QByteArray sha512(const QByteArray &a, const QByteArray &b);
QByteArray sha512(const QList<QByteArray> &parts);

QByteArray hkdfSha512(const QByteArray &ikm, const QByteArray &salt, const QByteArray &info, int length = 32);

void ed25519PublicKey(const uint8_t seed[32], uint8_t pub[32]);
QByteArray ed25519Sign(const QByteArray &seed, const QByteArray &message);
bool ed25519Verify(const QByteArray &publicKey, const QByteArray &message, const QByteArray &signature);

struct X25519KeyPair {
    QByteArray privateKey;
    QByteArray publicKey;
};
X25519KeyPair x25519Generate();
QByteArray x25519Shared(const QByteArray &privateKey, const QByteArray &peerPublic);

QByteArray nonceLabel(const char *label);
QByteArray nonce64(quint64 counter);
QByteArray chachaSeal(const QByteArray &key, const QByteArray &nonce12, const QByteArray &plaintext,
                      const QByteArray &aad = QByteArray());
QByteArray chachaOpen(const QByteArray &key, const QByteArray &nonce12, const QByteArray &ciphertextAndTag,
                      const QByteArray &aad = QByteArray());
QByteArray sha1(const QList<QByteArray> &parts);
QByteArray sha256(const QList<QByteArray> &parts);
QByteArray aesCtr128(const QByteArray &key16, const QByteArray &iv16, const QByteArray &data);

QByteArray concat(const QList<QByteArray> &parts);

}

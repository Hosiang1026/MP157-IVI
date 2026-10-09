#pragma once

#include <QByteArray>

class AirPlayControlCipher {
public:
    AirPlayControlCipher() = default;
    AirPlayControlCipher(const QByteArray &readKey, const QByteArray &writeKey);

    bool isValid() const { return m_readKey.size() == 32 && m_writeKey.size() == 32; }
    QByteArray encrypt(const QByteArray &plaintext);
    // Returns concatenated plaintext frames; leftover ciphertext stays in *rest.
    QByteArray decrypt(const QByteArray &buffer, QByteArray *rest);

private:
    QByteArray m_readKey;
    QByteArray m_writeKey;
    quint64 m_readCounter = 0;
    quint64 m_writeCounter = 0;
};

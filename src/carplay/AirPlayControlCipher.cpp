#include "AirPlayControlCipher.hpp"

#include "AirPlayCrypto.hpp"

AirPlayControlCipher::AirPlayControlCipher(const QByteArray &readKey, const QByteArray &writeKey)
    : m_readKey(readKey)
    , m_writeKey(writeKey)
{
}

QByteArray AirPlayControlCipher::encrypt(const QByteArray &plaintext)
{
    QByteArray out;
    int offset = 0;
    do {
        const int n = qMin(0x4000, qMax(0, plaintext.size() - offset));
        const QByteArray chunk = plaintext.mid(offset, n);
        QByteArray header(2, 0);
        header[0] = char(chunk.size() & 0xff);
        header[1] = char((chunk.size() >> 8) & 0xff);
        const QByteArray sealed =
            AirPlayCrypto::chachaSeal(m_writeKey, AirPlayCrypto::nonce64(m_writeCounter), chunk, header);
        ++m_writeCounter;
        out.append(header);
        out.append(sealed);
        offset += 0x4000;
    } while (offset < plaintext.size());
    return out;
}

QByteArray AirPlayControlCipher::decrypt(const QByteArray &buffer, QByteArray *rest)
{
    QByteArray out;
    int offset = 0;
    while (buffer.size() - offset >= 2) {
        const int length = (quint8(buffer[offset]) | (quint8(buffer[offset + 1]) << 8));
        const int frameEnd = offset + 2 + length + 16;
        if (buffer.size() < frameEnd)
            break;
        const QByteArray aad = buffer.mid(offset, 2);
        const QByteArray sealed = buffer.mid(offset + 2, length + 16);
        const QByteArray plain =
            AirPlayCrypto::chachaOpen(m_readKey, AirPlayCrypto::nonce64(m_readCounter), sealed, aad);
        if (plain.isNull()) {
            if (rest)
                *rest = buffer.mid(offset);
            return QByteArray();
        }
        ++m_readCounter;
        out.append(plain);
        offset = frameEnd;
    }
    if (rest)
        *rest = buffer.mid(offset);
    return out;
}

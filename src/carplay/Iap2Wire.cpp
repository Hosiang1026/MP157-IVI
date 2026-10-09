#include "Iap2Wire.hpp"

#include <cstring>

namespace Iap2Wire {

static void writeU16(QByteArray &out, int offset, quint16 value)
{
    out[offset] = char((value >> 8) & 0xff);
    out[offset + 1] = char(value & 0xff);
}

static quint16 readU16(const QByteArray &data, int offset)
{
    return quint16((quint8(data[offset]) << 8) | quint8(data[offset + 1]));
}

QByteArray encodeParam(quint16 id, const QByteArray &payload)
{
    QByteArray out(4 + payload.size(), Qt::Uninitialized);
    writeU16(out, 0, quint16(out.size()));
    writeU16(out, 2, id);
    if (!payload.isEmpty())
        memcpy(out.data() + 4, payload.constData(), size_t(payload.size()));
    return out;
}

QByteArray encodeFrame(quint16 messageId, const QByteArray &body)
{
    QByteArray out(kCsmHeaderBytes + body.size(), Qt::Uninitialized);
    writeU16(out, 0, quint16(kCsmStart));
    writeU16(out, 2, quint16(out.size()));
    writeU16(out, 4, messageId);
    if (!body.isEmpty())
        memcpy(out.data() + kCsmHeaderBytes, body.constData(), size_t(body.size()));
    return out;
}

QByteArray accessoryCertificateFrame(const QByteArray &certificate)
{
    return encodeFrame(quint16(kAccessoryCertificate), encodeParam(0, certificate));
}

QByteArray challengeResponseFrame(const QByteArray &signature)
{
    return encodeFrame(quint16(kChallengeResponse), encodeParam(0, signature));
}

QList<Frame> CsmFramer::offer(const QByteArray &chunk)
{
    QList<Frame> frames;
    if (!chunk.isEmpty())
        m_buf.append(chunk);

    while (true) {
        while (m_buf.size() >= 2
               && !(quint8(m_buf[0]) == 0x40 && quint8(m_buf[1]) == 0x40)) {
            m_buf.remove(0, 1);
        }
        if (m_buf.size() < kCsmHeaderBytes)
            break;

        const quint16 length = readU16(m_buf, 2);
        if (length < kCsmHeaderBytes) {
            m_buf.remove(0, 1);
            continue;
        }
        if (m_buf.size() < int(length))
            break;

        Frame frame;
        frame.messageId = readU16(m_buf, 4);
        frame.body = m_buf.mid(kCsmHeaderBytes, int(length) - kCsmHeaderBytes);
        frames.append(frame);
        m_buf.remove(0, int(length));
    }
    return frames;
}

} // namespace Iap2Wire

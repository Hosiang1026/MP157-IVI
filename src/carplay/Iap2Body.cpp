#include "Iap2Body.hpp"

#include "Iap2Wire.hpp"

Iap2Body &Iap2Body::addRaw(quint16 id, const QByteArray &payload)
{
    m_body.append(Iap2Wire::encodeParam(id, payload));
    return *this;
}

Iap2Body &Iap2Body::addString(quint16 id, const QString &value)
{
    QByteArray utf8 = value.toUtf8();
    utf8.append(char(0));
    return addRaw(id, utf8);
}

Iap2Body &Iap2Body::addBytes(quint16 id, const QByteArray &value)
{
    return addRaw(id, value);
}

Iap2Body &Iap2Body::addU8(quint16 id, quint8 value)
{
    QByteArray p(1, char(value));
    return addRaw(id, p);
}

Iap2Body &Iap2Body::addU16(quint16 id, quint16 value)
{
    QByteArray p(2, Qt::Uninitialized);
    p[0] = char((value >> 8) & 0xff);
    p[1] = char(value & 0xff);
    return addRaw(id, p);
}

Iap2Body &Iap2Body::addU32(quint16 id, quint32 value)
{
    QByteArray p(4, Qt::Uninitialized);
    p[0] = char((value >> 24) & 0xff);
    p[1] = char((value >> 16) & 0xff);
    p[2] = char((value >> 8) & 0xff);
    p[3] = char(value & 0xff);
    return addRaw(id, p);
}

Iap2Body &Iap2Body::addVoid(quint16 id)
{
    return addRaw(id, QByteArray());
}

Iap2Body &Iap2Body::addU16List(quint16 id, const QList<quint16> &values)
{
    QByteArray p(values.size() * 2, Qt::Uninitialized);
    for (int i = 0; i < values.size(); ++i) {
        const quint16 v = values[i];
        p[i * 2] = char((v >> 8) & 0xff);
        p[i * 2 + 1] = char(v & 0xff);
    }
    return addRaw(id, p);
}

Iap2Body &Iap2Body::addGroup(quint16 id, const std::function<void(Iap2Body &)> &build)
{
    Iap2Body child;
    if (build)
        build(child);
    return addRaw(id, child.encode());
}

QByteArray Iap2Body::encode() const
{
    return m_body;
}

bool Iap2Body::parseParams(const QByteArray &body, QVector<QPair<quint16, QByteArray>> *out)
{
    if (!out)
        return false;
    out->clear();
    int offset = 0;
    while (offset < body.size()) {
        if (body.size() - offset < 4)
            return false;
        const quint16 length = quint16((quint8(body[offset]) << 8) | quint8(body[offset + 1]));
        if (length < 4 || length > body.size() - offset)
            return false;
        const quint16 id = quint16((quint8(body[offset + 2]) << 8) | quint8(body[offset + 3]));
        out->append(qMakePair(id, body.mid(offset + 4, int(length) - 4)));
        offset += int(length);
    }
    return true;
}

QByteArray Iap2Body::firstParam(const QByteArray &body, quint16 id)
{
    QVector<QPair<quint16, QByteArray>> params;
    if (!parseParams(body, &params))
        return {};
    for (const auto &p : params) {
        if (p.first == id)
            return p.second;
    }
    return {};
}

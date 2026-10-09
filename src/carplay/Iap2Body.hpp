#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QVector>

#include <functional>

class Iap2Body {
public:
    Iap2Body &addString(quint16 id, const QString &value);
    Iap2Body &addBytes(quint16 id, const QByteArray &value);
    Iap2Body &addU8(quint16 id, quint8 value);
    Iap2Body &addU16(quint16 id, quint16 value);
    Iap2Body &addU32(quint16 id, quint32 value);
    Iap2Body &addVoid(quint16 id);
    Iap2Body &addU16List(quint16 id, const QList<quint16> &values);
    Iap2Body &addGroup(quint16 id, const std::function<void(Iap2Body &)> &build);

    QByteArray encode() const;

    static bool parseParams(const QByteArray &body, QVector<QPair<quint16, QByteArray>> *out);
    static QByteArray firstParam(const QByteArray &body, quint16 id);

private:
    Iap2Body &addRaw(quint16 id, const QByteArray &payload);

    QByteArray m_body;
};

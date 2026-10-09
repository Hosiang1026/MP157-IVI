#include "AirPlayBplist.hpp"

#include <QMetaType>

#include <cstring>
#include <functional>

namespace AirPlayBplist {
namespace {

void writeBe(QByteArray &out, quint64 value, int size)
{
    for (int i = size - 1; i >= 0; --i)
        out.append(char((value >> (8 * i)) & 0xff));
}

quint64 readBe(const QByteArray &bytes, int offset, int size)
{
    quint64 v = 0;
    for (int i = 0; i < size; ++i)
        v = (v << 8) | quint8(bytes[offset + i]);
    return v;
}

QByteArray bplistMarker(int type, int count)
{
    if (count < 0x0f)
        return QByteArray(1, char((type << 4) | count));
    int size = 1;
    if (count > 0xffff)
        size = 4;
    else if (count > 0xff)
        size = 2;
    const int log = size == 4 ? 2 : (size == 2 ? 1 : 0);
    QByteArray out;
    out.append(char((type << 4) | 0x0f));
    out.append(char(0x10 | log));
    writeBe(out, quint64(count), size);
    return out;
}

QByteArray bplistEncodeInt(qint64 number)
{
    int size = 1;
    if (number > 0xffffffffLL)
        size = 8;
    else if (number > 0xffff)
        size = 4;
    else if (number > 0xff)
        size = 2;
    const int log = size == 8 ? 3 : (size == 4 ? 2 : (size == 2 ? 1 : 0));
    QByteArray out;
    out.append(char(0x10 | log));
    writeBe(out, quint64(number), size);
    return out;
}

QByteArray bplistEncodeString(const QString &value)
{
    bool isAscii = true;
    for (const QChar &ch : value) {
        if (ch.unicode() > 0x7f) {
            isAscii = false;
            break;
        }
    }
    if (isAscii) {
        const QByteArray ascii = value.toLatin1();
        QByteArray out = bplistMarker(0x5, ascii.size());
        out.append(ascii);
        return out;
    }
    QByteArray be;
    const ushort *u = value.utf16();
    const int n = value.size();
    for (int i = 0; i < n; ++i) {
        be.append(char((u[i] >> 8) & 0xff));
        be.append(char(u[i] & 0xff));
    }
    QByteArray out = bplistMarker(0x6, n);
    out.append(be);
    return out;
}

QByteArray bplistEncodeData(const QByteArray &value)
{
    QByteArray out = bplistMarker(0x4, value.size());
    out.append(value);
    return out;
}

} // namespace

QByteArray encode(const QVariantMap &root)
{
    struct Node {
        QByteArray body;
        QList<int> refs;
        bool container = false;
    };
    QList<Node> nodes;

    std::function<int(const QVariant &)> add;
    add = [&](const QVariant &value) -> int {
        const int index = nodes.size();
        nodes.append(Node{});
        if (value.typeId() == QMetaType::Bool) {
            nodes[index].body = QByteArray(1, char(value.toBool() ? 0x09 : 0x08));
        } else if (value.typeId() == QMetaType::LongLong || value.typeId() == QMetaType::Int ||
                   value.typeId() == QMetaType::ULongLong || value.typeId() == QMetaType::UInt) {
            const qint64 number = value.toLongLong();
            if (number >= 0) {
                nodes[index].body = bplistEncodeInt(number);
            } else {
                const double d = double(number);
                quint64 bits = 0;
                static_assert(sizeof(bits) == sizeof(d));
                std::memcpy(&bits, &d, sizeof(bits));
                QByteArray body;
                body.append(char(0x23));
                writeBe(body, bits, 8);
                nodes[index].body = body;
            }
        } else if (value.typeId() == QMetaType::Double || value.typeId() == QMetaType::Float) {
            const double d = value.toDouble();
            quint64 bits = 0;
            std::memcpy(&bits, &d, sizeof(bits));
            QByteArray body;
            body.append(char(0x23));
            writeBe(body, bits, 8);
            nodes[index].body = body;
        } else if (value.typeId() == QMetaType::QString) {
            nodes[index].body = bplistEncodeString(value.toString());
        } else if (value.typeId() == QMetaType::QByteArray) {
            nodes[index].body = bplistEncodeData(value.toByteArray());
        } else if (value.typeId() == QMetaType::QVariantMap || value.canConvert<QVariantMap>()) {
            const QVariantMap map = value.toMap();
            QList<int> refs;
            for (auto it = map.constBegin(); it != map.constEnd(); ++it)
                refs.append(add(it.key()));
            for (auto it = map.constBegin(); it != map.constEnd(); ++it)
                refs.append(add(it.value()));
            nodes[index].container = true;
            nodes[index].body = bplistMarker(0xd, map.size());
            nodes[index].refs = refs;
        } else if (value.typeId() == QMetaType::QVariantList || value.canConvert<QVariantList>()) {
            const QVariantList list = value.toList();
            QList<int> refs;
            for (const QVariant &item : list)
                refs.append(add(item));
            nodes[index].container = true;
            nodes[index].body = bplistMarker(0xa, list.size());
            nodes[index].refs = refs;
        } else {
            nodes[index].body = bplistEncodeString(value.toString());
        }
        return index;
    };

    const int top = add(root);
    const int refSize = nodes.size() > 0xff ? 2 : 1;

    QByteArray parts = QByteArrayLiteral("bplist00");
    QList<int> offsets;
    for (const Node &node : nodes) {
        offsets.append(parts.size());
        parts.append(node.body);
        if (node.container) {
            for (int ref : node.refs)
                writeBe(parts, quint64(ref), refSize);
        }
    }
    const int offsetTable = parts.size();
    const int offsetSize = parts.size() > 0xffff ? 4 : (parts.size() > 0xff ? 2 : 1);
    for (int off : offsets)
        writeBe(parts, quint64(off), offsetSize);

    QByteArray trailer(32, 0);
    trailer[6] = char(offsetSize);
    trailer[7] = char(refSize);
    auto put64 = [&](int at, quint64 v) {
        for (int i = 7; i >= 0; --i)
            trailer[at + (7 - i)] = char((v >> (8 * i)) & 0xff);
    };
    put64(8, quint64(nodes.size()));
    put64(16, quint64(top));
    put64(24, quint64(offsetTable));
    parts.append(trailer);
    return parts;
}

QVariant decode(const QByteArray &bytes)
{
    if (bytes.size() < 40 || !bytes.startsWith("bplist00"))
        return {};
    const int trailer = bytes.size() - 32;
    const int offsetSize = quint8(bytes[trailer + 6]);
    const int refSize = quint8(bytes[trailer + 7]);
    const int numObjects = int(readBe(bytes, trailer + 8, 8));
    const int topObject = int(readBe(bytes, trailer + 16, 8));
    const qint64 offsetTable = qint64(readBe(bytes, trailer + 24, 8));
    if (numObjects <= 0 || offsetSize < 1 || refSize < 1)
        return {};

    QList<qint64> offsets;
    offsets.reserve(numObjects);
    for (int i = 0; i < numObjects; ++i)
        offsets.append(qint64(readBe(bytes, int(offsetTable + i * offsetSize), offsetSize)));

    std::function<QVariant(int)> readObject;
    readObject = [&](int index) -> QVariant {
        if (index < 0 || index >= offsets.size())
            return {};
        int position = int(offsets[index]);
        if (position < 0 || position >= bytes.size())
            return {};
        const int markerByte = quint8(bytes[position++]);
        const int type = markerByte >> 4;
        const int nibble = markerByte & 0x0f;

        auto readCount = [&]() -> int {
            if (nibble != 0x0f)
                return nibble;
            if (position >= bytes.size())
                return -1;
            const int sizeMarker = quint8(bytes[position++]);
            const int intBytes = 1 << (sizeMarker & 0x0f);
            if (position + intBytes > bytes.size())
                return -1;
            const int count = int(readBe(bytes, position, intBytes));
            position += intBytes;
            return count;
        };

        switch (type) {
        case 0x0:
            if (nibble == 0x08)
                return false;
            if (nibble == 0x09)
                return true;
            return {};
        case 0x1: {
            const int size = 1 << nibble;
            if (position + size > bytes.size())
                return {};
            return qlonglong(readBe(bytes, position, size));
        }
        case 0x2: {
            const int size = 1 << nibble;
            if (position + size > bytes.size())
                return {};
            if (size == 8) {
                const quint64 bits = readBe(bytes, position, 8);
                double d;
                std::memcpy(&d, &bits, 8);
                return d;
            }
            return {};
        }
        case 0x4: {
            const int count = readCount();
            if (count < 0 || position + count > bytes.size())
                return {};
            return bytes.mid(position, count);
        }
        case 0x5: {
            const int count = readCount();
            if (count < 0 || position + count > bytes.size())
                return {};
            return QString::fromLatin1(bytes.mid(position, count));
        }
        case 0x6: {
            const int count = readCount();
            if (count < 0 || position + count * 2 > bytes.size())
                return {};
            QString out;
            out.resize(count);
            for (int i = 0; i < count; ++i) {
                const ushort ch = ushort((quint8(bytes[position + i * 2]) << 8) | quint8(bytes[position + i * 2 + 1]));
                out[i] = QChar(ch);
            }
            return out;
        }
        case 0xa:
        case 0xc: {
            const int count = readCount();
            if (count < 0)
                return {};
            QVariantList list;
            for (int i = 0; i < count; ++i) {
                if (position + refSize > bytes.size())
                    return {};
                const int ref = int(readBe(bytes, position, refSize));
                position += refSize;
                list.append(readObject(ref));
            }
            return list;
        }
        case 0xd: {
            const int count = readCount();
            if (count < 0)
                return {};
            QVariantMap map;
            QList<int> keyRefs;
            for (int i = 0; i < count; ++i) {
                if (position + refSize > bytes.size())
                    return {};
                keyRefs.append(int(readBe(bytes, position, refSize)));
                position += refSize;
            }
            for (int i = 0; i < count; ++i) {
                if (position + refSize > bytes.size())
                    return {};
                const int valueRef = int(readBe(bytes, position, refSize));
                position += refSize;
                map.insert(readObject(keyRefs[i]).toString(), readObject(valueRef));
            }
            return map;
        }
        default:
            return {};
        }
    };

    return readObject(topObject);
}

} // namespace AirPlayBplist

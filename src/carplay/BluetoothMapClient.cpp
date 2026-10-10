#include "BluetoothMapClient.hpp"

#include "BluetoothMediaHub.hpp"
#include "BluetoothRfcomm.hpp"
#include "NotificationSession.hpp"

#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThread>
#include <QtConcurrent>

namespace {

QMutex &stateMutex()
{
    static QMutex m;
    return m;
}

QByteArray mapTargetUuid()
{
    static const quint8 kBytes[] = {
        0xbb, 0x58, 0x2b, 0x40, 0x42, 0x0c, 0x11, 0xdb,
        0xb0, 0xde, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66
    };
    return QByteArray(reinterpret_cast<const char *>(kBytes), 16);
}

QByteArray obexConnectReq()
{
    QByteArray p;
    p.append(char(0x80));
    p.append(char(0x00));
    p.append(char(0x1a));
    p.append(char(0x10));
    p.append(char(0x00));
    p.append(char(0x20));
    p.append(char(0x00));
    p.append(char(0x46));
    p.append(char(0x00));
    p.append(char(0x13));
    p.append(mapTargetUuid());
    return p;
}

QByteArray utf16beName(const QString &name)
{
    QByteArray out;
    for (QChar ch : name) {
        const ushort u = ch.unicode();
        out.append(char((u >> 8) & 0xff));
        out.append(char(u & 0xff));
    }
    out.append(char(0));
    out.append(char(0));
    return out;
}

QByteArray setPathReq(const QString &name)
{
    const QByteArray nm = utf16beName(name);
    QByteArray p;
    p.append(char(0x85));
    const int len = 5 + 3 + nm.size();
    p.append(char((len >> 8) & 0xff));
    p.append(char(len & 0xff));
    p.append(char(0x02));
    p.append(char(0x00));
    p.append(char(0x01));
    const int hl = 3 + nm.size();
    p.append(char((hl >> 8) & 0xff));
    p.append(char(hl & 0xff));
    p.append(nm);
    return p;
}

QByteArray getMsgListing()
{
    const QByteArray type = QByteArrayLiteral("x-bt/MAP-msg-listing");
    QByteArray app;
    app.append(char(0x01));
    app.append(char(0x02));
    app.append(char(0x00));
    app.append(char(0x08));
    app.append(char(0x02));
    app.append(char(0x01));
    app.append(char(0x01));

    QByteArray p;
    p.append(char(0x83));
    const int len = 3 + (3 + type.size() + 1) + (3 + app.size());
    p.append(char((len >> 8) & 0xff));
    p.append(char(len & 0xff));
    p.append(char(0x42));
    const int tl = 3 + type.size() + 1;
    p.append(char((tl >> 8) & 0xff));
    p.append(char(tl & 0xff));
    p.append(type);
    p.append(char(0));
    p.append(char(0x4c));
    const int al = 3 + app.size();
    p.append(char((al >> 8) & 0xff));
    p.append(char(al & 0xff));
    p.append(app);
    return p;
}

bool writeAll(BluetoothRfcomm &rfcomm, const QByteArray &data)
{
    if (!rfcomm.isOpen() || data.isEmpty())
        return false;
    qint64 off = 0;
    while (off < data.size()) {
        const qint64 n = rfcomm.write(data.constData() + off, data.size() - off);
        if (n <= 0)
            return false;
        off += n;
    }
    return true;
}

QByteArray readPacket(BluetoothRfcomm &rfcomm, int timeoutMs)
{
    QByteArray buf;
    int waited = 0;
    while (waited < timeoutMs) {
        const QByteArray chunk = rfcomm.read(4096);
        if (!chunk.isEmpty())
            buf.append(chunk);
        if (buf.size() >= 3) {
            const int len = (quint8(buf[1]) << 8) | quint8(buf[2]);
            if (len >= 3 && buf.size() >= len)
                return buf.left(len);
        }
        QThread::msleep(40);
        waited += 40;
    }
    return {};
}

bool expectSuccess(const QByteArray &packet)
{
    if (packet.isEmpty())
        return false;
    const quint8 code = quint8(packet[0]) & 0x7f;
    return code == 0x20 || code == 0x10;
}

QByteArray bodyOf(const QByteArray &packet)
{
    if (packet.size() < 3)
        return {};
    int i = 3;
    if ((quint8(packet[0]) & 0x7f) == 0x00 && packet.size() >= 7)
        i = 7;
    QByteArray body;
    while (i + 3 <= packet.size()) {
        const quint8 hi = quint8(packet[i]);
        const int hl = (quint8(packet[i + 1]) << 8) | quint8(packet[i + 2]);
        if (hl < 3 || i + hl > packet.size())
            break;
        if (hi == 0x48 || hi == 0x49)
            body.append(packet.mid(i + 3, hl - 3));
        i += hl;
    }
    return body;
}

QString xmlAttr(const QString &tag, const QString &key)
{
    const QRegularExpression re(QStringLiteral("%1\\s*=\\s*\"([^\"]*)\"").arg(key),
                                QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(tag);
    return m.hasMatch() ? m.captured(1) : QString();
}

bool openMapSession(BluetoothRfcomm &rfcomm, const QString &address)
{
    if (!rfcomm.connectToUuid(address, QString::fromLatin1(BluetoothRfcomm::kMapUuid)))
        return false;
    if (!writeAll(rfcomm, obexConnectReq()) || !expectSuccess(readPacket(rfcomm, 5000)))
        return false;
    if (!writeAll(rfcomm, setPathReq(QStringLiteral("telecom")))
        || !expectSuccess(readPacket(rfcomm, 4000)))
        return false;
    if (!writeAll(rfcomm, setPathReq(QStringLiteral("msg")))
        || !expectSuccess(readPacket(rfcomm, 4000)))
        return false;
    if (!writeAll(rfcomm, setPathReq(QStringLiteral("inbox")))
        || !expectSuccess(readPacket(rfcomm, 4000)))
        return false;
    return true;
}

QByteArray fetchListing(BluetoothRfcomm &rfcomm)
{
    if (!writeAll(rfcomm, getMsgListing()))
        return {};
    QByteArray xml;
    for (int guard = 0; guard < 8; ++guard) {
        const QByteArray packet = readPacket(rfcomm, 6000);
        if (packet.isEmpty())
            return xml;
        xml.append(bodyOf(packet));
        const quint8 code = quint8(packet[0]) & 0x7f;
        if (code == 0x20)
            break;
        if (code != 0x10)
            break;
        QByteArray cont;
        cont.append(char(0x83));
        cont.append(char(0x00));
        cont.append(char(0x03));
        if (!writeAll(rfcomm, cont))
            break;
    }
    return xml;
}

} // namespace

BluetoothMapClient::BluetoothMapClient(BluetoothMediaHub *hub, NotificationSession *notifications,
                                       QObject *parent)
    : QObject(parent)
    , m_hub(hub)
    , m_notifications(notifications)
{
    m_timer.setInterval(6000);
    connect(&m_timer, &QTimer::timeout, this, &BluetoothMapClient::tick);
    if (m_hub) {
        connect(m_hub, &BluetoothMediaHub::phoneChanged, this, &BluetoothMapClient::tick);
        connect(m_hub, &BluetoothMediaHub::devicesChanged, this, &BluetoothMapClient::tick);
    }
    m_timer.start();
}

void BluetoothMapClient::tick()
{
    if (!m_hub || !m_notifications)
        return;
    if (!m_hub->phoneConnected() || m_hub->phoneAddress().isEmpty()) {
        QMutexLocker lock(&stateMutex());
        m_address.clear();
        m_seeded = false;
        m_seen.clear();
        return;
    }
    if (m_busy.exchange(true))
        return;

    const QString address = m_hub->phoneAddress();
    QtConcurrent::run([this, address] {
        BluetoothRfcomm rfcomm;
        if (!openMapSession(rfcomm, address)) {
            m_busy = false;
            return;
        }
        const QByteArray xml = fetchListing(rfcomm);
        rfcomm.disconnectFromHost();
        if (xml.isEmpty()) {
            m_busy = false;
            return;
        }

        const QString text = QString::fromUtf8(xml);
        const QRegularExpression re(QStringLiteral("<msg\\b[^>]*>"),
                                    QRegularExpression::CaseInsensitiveOption);
        auto it = re.globalMatch(text);
        struct Item { QString title; QString body; };
        QList<Item> fresh;
        {
            QMutexLocker lock(&stateMutex());
            if (m_address != address) {
                m_address = address;
                m_seeded = false;
                m_seen.clear();
            }
            while (it.hasNext()) {
                const QString tag = it.next().captured(0);
                const QString handle = xmlAttr(tag, QStringLiteral("handle"));
                if (handle.isEmpty() || m_seen.contains(handle))
                    continue;
                m_seen.insert(handle);
                const QString sender = xmlAttr(tag, QStringLiteral("sender_name"));
                const QString addr = xmlAttr(tag, QStringLiteral("sender_addressing"));
                const QString subject = xmlAttr(tag, QStringLiteral("subject"));
                Item item;
                item.title = !sender.isEmpty() ? sender
                             : (!addr.isEmpty() ? addr : QStringLiteral("短信"));
                item.body = !subject.isEmpty() ? subject : QStringLiteral("新消息");
                fresh.append(item);
            }
            if (!m_seeded) {
                m_seeded = true;
                fresh.clear();
            }
        }
        for (const Item &item : fresh) {
            QMetaObject::invokeMethod(m_notifications, "applyRemote", Qt::QueuedConnection,
                                      Q_ARG(QString, QStringLiteral("短信")),
                                      Q_ARG(QString, item.title),
                                      Q_ARG(QString, item.body));
        }
        m_busy = false;
    });
}

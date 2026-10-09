#include "BonjourAdvertiser.hpp"

#include <QMetaObject>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QProcess>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QThread>

namespace {

const char kAirPlayType[] = "_airplay._tcp.local";
const char kCarPlayCtrlType[] = "_carplay-ctrl._tcp.local";
const quint16 kMdnsPort = 5353;

} // namespace

BonjourAdvertiser::BonjourAdvertiser(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &BonjourAdvertiser::onReadyRead);
    m_browseTimer.setInterval(2000);
    connect(&m_browseTimer, &QTimer::timeout, this, &BonjourAdvertiser::browseCarPlayCtrl);
    m_preferTimer.setSingleShot(true);
    m_preferTimer.setInterval(2500);
    connect(&m_preferTimer, &QTimer::timeout, this, [this] {
        if (m_preferWaiting)
            finishPreferredProbe(m_preferHit);
    });
}

BonjourAdvertiser::~BonjourAdvertiser()
{
    stop();
}

bool BonjourAdvertiser::start(const QString &name, quint16 port, const QMap<QString, QString> &txt,
                              const QHostAddress &bindAddress, const QString &deviceId,
                              const QString &sourceVersion)
{
    stop();
    m_port = port;
    m_txt = txt;
    m_serviceType = QString::fromLatin1(kAirPlayType);
    m_instanceName = name + QLatin1Char('.') + m_serviceType;
    m_ipv4 = bindAddress;
    m_deviceId = deviceId;
    m_sourceVersion = sourceVersion.isEmpty() ? QStringLiteral("950.7.1") : sourceVersion;
    m_probed.clear();
    m_iface = QNetworkInterface();
    if (m_ipv4.isNull() || m_ipv4 == QHostAddress::AnyIPv4 || m_ipv4 == QHostAddress::Any) {
        emit failed(QStringLiteral("Bonjour requires concrete IPv4 bindAddress"));
        return false;
    }
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip() == m_ipv4) {
                m_iface = iface;
                break;
            }
        }
        if (m_iface.isValid())
            break;
    }

    QByteArray blob;
    for (auto it = txt.constBegin(); it != txt.constEnd(); ++it) {
        const QByteArray entry = (it.key() + QLatin1Char('=') + it.value()).toUtf8();
        if (entry.size() > 255)
            continue;
        blob.append(char(entry.size()));
        blob.append(entry);
    }
    m_txtBlob = blob;

    if (!m_socket.bind(QHostAddress::AnyIPv4, kMdnsPort,
                       QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit failed(QStringLiteral("mDNS bind 5353 failed: %1").arg(m_socket.errorString()));
        return false;
    }
    const QHostAddress group(QStringLiteral("224.0.0.251"));
    if (m_iface.isValid()) {
        m_socket.joinMulticastGroup(group, m_iface);
        m_socket.setMulticastInterface(m_iface);
    } else {
        m_socket.joinMulticastGroup(group);
    }
    m_running = true;
    sendAnnouncement();
    browseCarPlayCtrl();
    m_browseTimer.start();
    emit log(QStringLiteral("mDNS advertising %1 on %2:%3 iface=%4")
                 .arg(m_instanceName, m_ipv4.toString())
                 .arg(m_port)
                 .arg(m_iface.isValid() ? m_iface.humanReadableName() : QStringLiteral("any")));
    return true;
}

void BonjourAdvertiser::setPreferredHost(const QString &host)
{
    m_preferredHost = host.trimmed();
}

void BonjourAdvertiser::finishPreferredProbe(bool hit)
{
    if (!m_preferWaiting)
        return;
    m_preferWaiting = false;
    m_preferPending = 0;
    m_preferTimer.stop();
    if (hit)
        return;
    const QString stale = m_preferredHost;
    if (!stale.isEmpty()) {
        emit log(QStringLiteral("preferred host stale %1 · fallback lan probe").arg(stale));
        m_preferredHost.clear();
        emit preferredHostInvalid(stale);
    }
    probeLanNeighbors();
}

void BonjourAdvertiser::browseNow()
{
    sendAnnouncement();
    browseCarPlayCtrl();
    if (m_preferredHost.isEmpty()) {
        probeLanNeighbors();
        return;
    }
    m_preferWaiting = true;
    m_preferHit = false;
    m_preferPending = 0;
    const QList<quint16> ports{49152, 49153, 49154, 49155, 7000, 5000, 47000};
    for (quint16 port : ports) {
        const QString key = m_preferredHost + QLatin1Char(':') + QString::number(port);
        m_probed.remove(key);
        ++m_preferPending;
        const QHostAddress addr(m_preferredHost);
        if (!addr.isNull())
            probeCarPlayCtrl(addr, port, true);
        else
            --m_preferPending;
    }
    if (m_preferPending <= 0) {
        finishPreferredProbe(false);
        return;
    }
    m_preferTimer.start();
}

void BonjourAdvertiser::probeHost(const QString &host, quint16 port)
{
    if (!m_running || host.isEmpty() || m_deviceId.isEmpty())
        return;
    const QHostAddress addr(host);
    if (addr.isNull())
        return;
    probeCarPlayCtrl(addr, port, false);
}

void BonjourAdvertiser::probeLanNeighbors()
{
    if (!m_running || m_deviceId.isEmpty() || m_ipv4.isNull() || m_lanProbeBusy)
        return;

    QProcess arp;
    arp.start(QStringLiteral("arp"), {QStringLiteral("-a")});
    if (!arp.waitForFinished(2000))
        arp.kill();
    const QString arpOut = QString::fromLocal8Bit(arp.readAllStandardOutput());
    const quint32 mine = m_ipv4.toIPv4Address();
    const quint32 base = mine & 0xffffff00u;
    QList<QHostAddress> hosts;
    const QRegularExpression ipRe(QStringLiteral("(\\d+\\.\\d+\\.\\d+\\.\\d+)"));
    for (const QRegularExpressionMatch &m : ipRe.globalMatch(arpOut)) {
        const QHostAddress a(m.captured(1));
        const quint32 v = a.toIPv4Address();
        if ((v & 0xffffff00u) == base && v != mine && !hosts.contains(a))
            hosts.append(a);
    }
    if (hosts.isEmpty())
        return;

    const QList<quint16> ports{49152, 49153, 49154, 49155, 7000, 5000, 47000};
    emit log(QStringLiteral("lan probe start hosts=%1").arg(hosts.size()));
    m_lanProbeBusy = true;
    const QString deviceId = m_deviceId;
    const QString sourceVersion = m_sourceVersion;
    const QHostAddress bind = m_ipv4;
    QThread *thread = QThread::create([this, hosts, ports, deviceId, sourceVersion, bind] {
        int hits = 0;
        for (const QHostAddress &host : hosts) {
            for (quint16 port : ports) {
                QTcpSocket sock;
                sock.bind(bind, 0);
                sock.connectToHost(host, port);
                if (!sock.waitForConnected(120))
                    continue;
                const QString receiverId = QString(deviceId).remove(QLatin1Char(':'));
                const QByteArray req =
                    QStringLiteral("GET /ctrl-int/1/connect HTTP/1.1\r\n"
                                   "Host: %1:%2\r\n"
                                   "User-Agent: AirPlay/%3\r\n"
                                   "AirPlay-Receiver-Device-ID: %4\r\n"
                                   "Connection: close\r\n"
                                   "\r\n")
                        .arg(host.toString())
                        .arg(port)
                        .arg(sourceVersion, receiverId)
                        .toLatin1();
                sock.write(req);
                sock.flush();
                sock.waitForReadyRead(400);
                const QByteArray resp = sock.readLine().trimmed();
                sock.close();
                if (resp.startsWith("HTTP/")) {
                    ++hits;
                    QMetaObject::invokeMethod(this, [this, host, port, resp] {
                        emit log(QStringLiteral("lan probe hit %1:%2 %3")
                                     .arg(host.toString())
                                     .arg(port)
                                     .arg(QString::fromLatin1(resp)));
                        emit ctrlProbed(host.toString(), port, QString::fromLatin1(resp));
                    }, Qt::QueuedConnection);
                }
            }
        }
        QMetaObject::invokeMethod(this, [this, hits] {
            m_lanProbeBusy = false;
            emit log(QStringLiteral("lan probe done hits=%1").arg(hits));
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void BonjourAdvertiser::stop()
{
    m_browseTimer.stop();
    m_preferTimer.stop();
    m_preferWaiting = false;
    m_preferPending = 0;
    m_preferHit = false;
    if (!m_running)
        return;
    const QHostAddress group(QStringLiteral("224.0.0.251"));
    if (m_iface.isValid())
        m_socket.leaveMulticastGroup(group, m_iface);
    else
        m_socket.leaveMulticastGroup(group);
    m_socket.close();
    m_running = false;
    m_lanProbeBusy = false;
    m_probed.clear();
}

void BonjourAdvertiser::sendDatagram(const QByteArray &pkt)
{
    QNetworkDatagram dg(pkt, QHostAddress(QStringLiteral("224.0.0.251")), kMdnsPort);
    if (m_iface.isValid())
        dg.setInterfaceIndex(m_iface.index());
    m_socket.writeDatagram(dg);
}

quint16 BonjourAdvertiser::readU16(const quint8 *p)
{
    return quint16((quint16(p[0]) << 8) | p[1]);
}

void BonjourAdvertiser::writeU16(QByteArray &out, quint16 v)
{
    out.append(char((v >> 8) & 0xff));
    out.append(char(v & 0xff));
}

void BonjourAdvertiser::writeU32(QByteArray &out, quint32 v)
{
    out.append(char((v >> 24) & 0xff));
    out.append(char((v >> 16) & 0xff));
    out.append(char((v >> 8) & 0xff));
    out.append(char(v & 0xff));
}

bool BonjourAdvertiser::skipDnsName(const QByteArray &packet, int &off)
{
    while (off < packet.size()) {
        const quint8 len = quint8(packet[off]);
        if (len == 0) {
            ++off;
            return true;
        }
        if ((len & 0xc0) == 0xc0) {
            off += 2;
            return true;
        }
        off += 1 + len;
        if (off > packet.size())
            return false;
    }
    return false;
}

QString BonjourAdvertiser::readDnsName(const QByteArray &packet, int off)
{
    QStringList labels;
    int guard = 0;
    while (off < packet.size() && guard++ < 64) {
        const quint8 len = quint8(packet[off]);
        if (len == 0)
            break;
        if ((len & 0xc0) == 0xc0) {
            if (off + 1 >= packet.size())
                break;
            off = ((len & 0x3f) << 8) | quint8(packet[off + 1]);
            continue;
        }
        ++off;
        if (off + len > packet.size())
            break;
        labels.append(QString::fromUtf8(packet.constData() + off, len));
        off += len;
    }
    return labels.join(QLatin1Char('.'));
}

QByteArray BonjourAdvertiser::encodeDnsName(const QString &name) const
{
    QByteArray out;
    const QStringList labels = name.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    for (const QString &label : labels) {
        const QByteArray b = label.toUtf8();
        if (b.size() > 63)
            continue;
        out.append(char(b.size()));
        out.append(b);
    }
    out.append(char(0));
    return out;
}

void BonjourAdvertiser::sendPtrQuery(const QString &serviceType)
{
    QByteArray pkt;
    writeU16(pkt, 0);
    writeU16(pkt, 0x0000);
    writeU16(pkt, 1);
    writeU16(pkt, 0);
    writeU16(pkt, 0);
    writeU16(pkt, 0);
    pkt.append(encodeDnsName(serviceType));
    writeU16(pkt, 12);
    writeU16(pkt, 1);
    sendDatagram(pkt);
}

void BonjourAdvertiser::browseCarPlayCtrl()
{
    if (!m_running)
        return;
    sendPtrQuery(QString::fromLatin1(kCarPlayCtrlType));
    sendAnnouncement();
}

void BonjourAdvertiser::sendAnnouncement()
{
    QByteArray pkt;
    writeU16(pkt, 0);
    writeU16(pkt, 0x8400);
    writeU16(pkt, 0);
    writeU16(pkt, 4);
    writeU16(pkt, 0);
    writeU16(pkt, 0);

    const QByteArray ptrName = encodeDnsName(m_serviceType);
    const QByteArray srvName = encodeDnsName(m_instanceName);
    const QByteArray hostName = encodeDnsName(m_ipv4.toString().replace(QLatin1Char('.'), QLatin1Char('-')) +
                                              QStringLiteral(".local"));

    auto appendRR = [&](const QByteArray &owner, quint16 type, const QByteArray &rdata) {
        pkt.append(owner);
        writeU16(pkt, type);
        writeU16(pkt, 0x8001);
        writeU32(pkt, 120);
        writeU16(pkt, quint16(rdata.size()));
        pkt.append(rdata);
    };

    appendRR(ptrName, 12, srvName);

    QByteArray srvRdata;
    writeU16(srvRdata, 0);
    writeU16(srvRdata, 0);
    writeU16(srvRdata, m_port);
    srvRdata.append(hostName);
    appendRR(srvName, 33, srvRdata);

    appendRR(srvName, 16, m_txtBlob);

    QByteArray aRdata;
    const quint32 ip = m_ipv4.toIPv4Address();
    writeU32(aRdata, ip);
    appendRR(hostName, 1, aRdata);

    sendDatagram(pkt);
}

QByteArray BonjourAdvertiser::buildResponse(const QByteArray &query) const
{
    if (query.size() < 12)
        return {};
    const auto *p = reinterpret_cast<const quint8 *>(query.constData());
    const quint16 flags = readU16(p + 2);
    if (flags & 0x8000)
        return {};
    const quint16 qdcount = readU16(p + 4);
    int off = 12;
    bool wantPtr = false, wantSrv = false, wantTxt = false, wantA = false;

    for (quint16 i = 0; i < qdcount; ++i) {
        const int nameOff = off;
        if (!skipDnsName(query, off) || off + 4 > query.size())
            return {};
        const quint16 qtype = readU16(reinterpret_cast<const quint8 *>(query.constData()) + off);
        off += 4;
        const QString qname = readDnsName(query, nameOff).toLower();
        const QString service = m_serviceType.toLower();
        const QString instance = m_instanceName.toLower();
        if (qname == service && (qtype == 12 || qtype == 255))
            wantPtr = true;
        if (qname == instance && (qtype == 33 || qtype == 255))
            wantSrv = true;
        if (qname == instance && (qtype == 16 || qtype == 255))
            wantTxt = true;
        if (qtype == 1 || qtype == 255)
            wantA = true;
        if (qtype == 255) {
            wantPtr = true;
            wantSrv = true;
            wantTxt = true;
            wantA = true;
        }
    }
    if (!wantPtr && !wantSrv && !wantTxt && !wantA)
        return {};

    QByteArray pkt;
    writeU16(pkt, readU16(p));
    writeU16(pkt, 0x8400);
    writeU16(pkt, 0);
    const int anCountPos = pkt.size();
    writeU16(pkt, 0);
    writeU16(pkt, 0);
    writeU16(pkt, 0);

    const QByteArray ptrName = encodeDnsName(m_serviceType);
    const QByteArray srvName = encodeDnsName(m_instanceName);
    const QByteArray hostName = encodeDnsName(m_ipv4.toString().replace(QLatin1Char('.'), QLatin1Char('-')) +
                                              QStringLiteral(".local"));
    quint16 answers = 0;

    auto appendRR = [&](const QByteArray &owner, quint16 type, const QByteArray &rdata) {
        pkt.append(owner);
        writeU16(pkt, type);
        writeU16(pkt, 0x8001);
        writeU32(pkt, 120);
        writeU16(pkt, quint16(rdata.size()));
        pkt.append(rdata);
        ++answers;
    };

    if (wantPtr)
        appendRR(ptrName, 12, srvName);
    if (wantSrv) {
        QByteArray srvRdata;
        writeU16(srvRdata, 0);
        writeU16(srvRdata, 0);
        writeU16(srvRdata, m_port);
        srvRdata.append(hostName);
        appendRR(srvName, 33, srvRdata);
    }
    if (wantTxt)
        appendRR(srvName, 16, m_txtBlob);
    if (wantA) {
        QByteArray aRdata;
        writeU32(aRdata, m_ipv4.toIPv4Address());
        appendRR(hostName, 1, aRdata);
    }

    pkt[anCountPos] = char((answers >> 8) & 0xff);
    pkt[anCountPos + 1] = char(answers & 0xff);
    return pkt;
}

void BonjourAdvertiser::parseIncoming(const QByteArray &packet, const QHostAddress &sender)
{
    if (packet.size() < 12)
        return;
    const auto *p = reinterpret_cast<const quint8 *>(packet.constData());
    const quint16 flags = readU16(p + 2);
    const quint16 qdcount = readU16(p + 4);
    const quint16 ancount = readU16(p + 6);
    const quint16 nscount = readU16(p + 8);
    const quint16 arcount = readU16(p + 10);
    int off = 12;

    for (quint16 i = 0; i < qdcount; ++i) {
        if (!skipDnsName(packet, off) || off + 4 > packet.size())
            return;
        off += 4;
    }

    QMap<QString, quint16> srvPorts;
    QMap<QString, QHostAddress> hostIps;
    QStringList ctrlInstances;

    const quint16 total = ancount + nscount + arcount;
    for (quint16 i = 0; i < total; ++i) {
        const int nameOff = off;
        if (!skipDnsName(packet, off) || off + 10 > packet.size())
            return;
        const quint16 type = readU16(p + off);
        off += 8;
        const quint16 rdlen = readU16(p + off);
        off += 2;
        if (off + rdlen > packet.size())
            return;
        const QString owner = readDnsName(packet, nameOff).toLower();
        if (type == 12 && owner.contains(QStringLiteral("_carplay-ctrl._tcp"))) {
            const QString target = readDnsName(packet, off).toLower();
            if (!target.isEmpty())
                ctrlInstances.append(target);
        } else if (type == 33 && rdlen >= 6) {
            const quint16 port = readU16(p + off + 4);
            srvPorts.insert(owner, port);
        } else if (type == 1 && rdlen == 4) {
            const quint32 ip = (quint32(quint8(packet[off])) << 24)
                             | (quint32(quint8(packet[off + 1])) << 16)
                             | (quint32(quint8(packet[off + 2])) << 8)
                             | quint32(quint8(packet[off + 3]));
            hostIps.insert(owner, QHostAddress(ip));
        }
        off += rdlen;
    }

    for (const QString &instance : ctrlInstances) {
        const quint16 port = srvPorts.value(instance);
        if (port == 0)
            continue;
        QHostAddress host = hostIps.value(instance);
        if (host.isNull()) {
            for (auto it = hostIps.constBegin(); it != hostIps.constEnd(); ++it) {
                host = it.value();
                break;
            }
        }
        if (host.isNull() && sender.protocol() == QAbstractSocket::IPv4Protocol)
            host = sender;
        if (host.isNull())
            continue;
        probeCarPlayCtrl(host, port);
    }
}

void BonjourAdvertiser::probeCarPlayCtrl(const QHostAddress &host, quint16 port, bool preferred)
{
    const QString key = host.toString() + QLatin1Char(':') + QString::number(port);
    if (m_probed.contains(key) || m_deviceId.isEmpty()) {
        if (preferred && m_preferWaiting) {
            --m_preferPending;
            if (m_preferPending <= 0)
                finishPreferredProbe(m_preferHit);
        }
        return;
    }
    m_probed.insert(key);

    const QString deviceId = m_deviceId;
    const QString sourceVersion = m_sourceVersion;
    const QHostAddress bind = m_ipv4;
    QThread *thread = QThread::create([this, host, port, deviceId, sourceVersion, bind, key, preferred] {
        QTcpSocket sock;
        sock.bind(bind, 0);
        sock.connectToHost(host, port);
        if (!sock.waitForConnected(preferred ? 800 : 3000)) {
            QMetaObject::invokeMethod(this, [this, key, host, port, preferred, err = sock.errorString()] {
                m_probed.remove(key);
                emit log(QStringLiteral("carplay-ctrl probe fail %1:%2 %3")
                             .arg(host.toString())
                             .arg(port)
                             .arg(err));
                if (preferred && m_preferWaiting) {
                    --m_preferPending;
                    if (m_preferPending <= 0)
                        finishPreferredProbe(m_preferHit);
                }
            }, Qt::QueuedConnection);
            return;
        }
        const QString receiverId = QString(deviceId).remove(QLatin1Char(':'));
        const QByteArray req =
            QStringLiteral("GET /ctrl-int/1/connect HTTP/1.1\r\n"
                           "Host: %1:%2\r\n"
                           "User-Agent: AirPlay/%3\r\n"
                           "AirPlay-Receiver-Device-ID: %4\r\n"
                           "Connection: close\r\n"
                           "\r\n")
                .arg(host.toString())
                .arg(port)
                .arg(sourceVersion, receiverId)
                .toLatin1();
        sock.write(req);
        sock.flush();
        sock.waitForReadyRead(preferred ? 800 : 3000);
        const QByteArray resp = sock.readLine().trimmed();
        sock.close();
        const bool ok = resp.startsWith("HTTP/");
        QMetaObject::invokeMethod(this, [this, host, port, resp, preferred, ok] {
            emit log(QStringLiteral("carplay-ctrl probe %1:%2 %3")
                         .arg(host.toString())
                         .arg(port)
                         .arg(QString::fromLatin1(resp)));
            if (ok) {
                emit ctrlProbed(host.toString(), port, QString::fromLatin1(resp));
                if (m_preferWaiting) {
                    m_preferHit = true;
                    m_preferredHost = host.toString();
                    if (preferred)
                        --m_preferPending;
                    finishPreferredProbe(true);
                    return;
                }
            }
            if (preferred && m_preferWaiting) {
                --m_preferPending;
                if (m_preferPending <= 0)
                    finishPreferredProbe(m_preferHit);
            }
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void BonjourAdvertiser::onReadyRead()
{
    while (m_socket.hasPendingDatagrams()) {
        const QNetworkDatagram dg = m_socket.receiveDatagram();
        parseIncoming(dg.data(), dg.senderAddress());
        const QByteArray resp = buildResponse(dg.data());
        if (resp.isEmpty())
            continue;
        sendDatagram(resp);
    }
}

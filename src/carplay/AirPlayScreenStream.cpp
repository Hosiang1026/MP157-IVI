#include "AirPlayScreenStream.hpp"

#include "AirPlayCrypto.hpp"

#include <QHostAddress>

namespace {
constexpr int kHeader = 128;
}

AirPlayScreenStream::AirPlayScreenStream(const QByteArray &key, QObject *parent)
    : QObject(parent)
    , m_key(key)
{
    connect(&m_server, &QTcpServer::newConnection, this, &AirPlayScreenStream::onNewConnection);
}

AirPlayScreenStream::~AirPlayScreenStream()
{
    stop();
}

quint16 AirPlayScreenStream::start()
{
    stop();
    if (!m_server.listen(QHostAddress::AnyIPv4, 0))
        return 0;
    m_port = m_server.serverPort();
    return m_port;
}

void AirPlayScreenStream::stop()
{
    if (m_sock) {
        m_sock->disconnect(this);
        m_sock->abort();
        m_sock->deleteLater();
        m_sock = nullptr;
    }
    m_server.close();
    m_buf.clear();
    m_frameCounter = 0;
    m_port = 0;
}

void AirPlayScreenStream::onNewConnection()
{
    while (auto *sock = m_server.nextPendingConnection()) {
        if (m_sock) {
            sock->abort();
            sock->deleteLater();
            continue;
        }
        m_sock = sock;
        connect(m_sock, &QTcpSocket::readyRead, this, &AirPlayScreenStream::onReadyRead);
        connect(m_sock, &QTcpSocket::disconnected, this, &AirPlayScreenStream::onDisconnected);
        emit log(QStringLiteral("ScreenStream connected from %1").arg(m_sock->peerAddress().toString()));
    }
}

void AirPlayScreenStream::onReadyRead()
{
    if (!m_sock)
        return;
    do {
        m_buf.append(m_sock->readAll());
        process();
    } while (m_sock && m_sock->bytesAvailable() > 0);
}

void AirPlayScreenStream::onDisconnected()
{
    emit closed(QStringLiteral("screen stream disconnected"));
    if (m_sock) {
        m_sock->deleteLater();
        m_sock = nullptr;
    }
}

void AirPlayScreenStream::process()
{
    while (m_buf.size() >= kHeader) {
        const quint32 payloadSize = quint8(m_buf[0]) | (quint8(m_buf[1]) << 8) | (quint8(m_buf[2]) << 16)
            | (quint8(m_buf[3]) << 24);
        if (payloadSize > 8 * 1024 * 1024) {
            emit log(QStringLiteral("bad screen payload size=%1 head=%2")
                         .arg(payloadSize)
                         .arg(QString::fromLatin1(m_buf.left(16).toHex())));
            emit closed(QStringLiteral("bad screen payload size"));
            m_buf.clear();
            return;
        }
        if (m_buf.size() < kHeader + int(payloadSize))
            return;

        const int payloadType = quint8(m_buf[4]);
        const QByteArray header = m_buf.left(kHeader);
        QByteArray payload = m_buf.mid(kHeader, int(payloadSize));
        m_buf.remove(0, kHeader + int(payloadSize));

        if (payloadSize == 0)
            continue;

        if (payloadType == 1) {
            emit log(QStringLiteral("screen config bytes=%1").arg(payload.size()));
            emit config(payload);
        } else if (payloadType == 0) {
            if (m_key.size() == 32 && payload.size() >= 16) {
                const QByteArray plain =
                    AirPlayCrypto::chachaOpen(m_key, AirPlayCrypto::nonce64(m_frameCounter), payload, header);
                if (plain.isNull()) {
                    emit log(QStringLiteral("screen decrypt failed ctr=%1 sealed=%2 head=%3")
                                 .arg(m_frameCounter)
                                 .arg(payload.size())
                                 .arg(QString::fromLatin1(header.left(16).toHex())));
                    emit closed(QStringLiteral("screen decrypt failed"));
                    return;
                }
                payload = plain;
                ++m_frameCounter;
                if (m_frameCounter <= 3 || (m_frameCounter % 60) == 0)
                    emit log(QStringLiteral("screen frame #%1 bytes=%2").arg(m_frameCounter).arg(payload.size()));
            }
            emit frame(toAnnexB(payload));
        } else {
            emit log(QStringLiteral("screen opcode=%1 bytes=%2").arg(payloadType).arg(payload.size()));
        }
    }
}

QByteArray AirPlayScreenStream::toAnnexB(QByteArray payload)
{
    if (payload.size() >= 4 && payload[0] == 0 && payload[1] == 0 && payload[2] == 0 && payload[3] == 1)
        return payload;
    int i = 0;
    int checked = 0;
    while (i + 4 <= payload.size()) {
        const int naluLen = (quint8(payload[i]) << 24) | (quint8(payload[i + 1]) << 16)
            | (quint8(payload[i + 2]) << 8) | quint8(payload[i + 3]);
        if (naluLen <= 0 || i + 4 + naluLen > payload.size())
            return payload;
        i += 4 + naluLen;
        ++checked;
    }
    if (checked == 0 || i != payload.size())
        return payload;
    i = 0;
    while (i + 4 <= payload.size()) {
        const int naluLen = (quint8(payload[i]) << 24) | (quint8(payload[i + 1]) << 16)
            | (quint8(payload[i + 2]) << 8) | quint8(payload[i + 3]);
        payload[i] = 0;
        payload[i + 1] = 0;
        payload[i + 2] = 0;
        payload[i + 3] = 1;
        i += 4 + naluLen;
    }
    return payload;
}

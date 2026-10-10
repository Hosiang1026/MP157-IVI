#include "BluetoothRfcomm.hpp"

#include <QStringList>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <winsock2.h>
#include <ws2bth.h>
#include <bluetoothapis.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Bthprops.lib")

namespace {
constexpr qintptr kInvalidSock = -1;
SOCKET asSock(qintptr s) { return SOCKET(s); }
} // namespace
#endif

#ifdef Q_OS_LINUX
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <QProcess>
#include <QRegularExpression>

#ifndef AF_BLUETOOTH
#define AF_BLUETOOTH 31
#endif
#ifndef BTPROTO_RFCOMM
#define BTPROTO_RFCOMM 3
#endif

namespace {

struct bdaddr_t {
    quint8 b[6];
};

struct sockaddr_rc {
    sa_family_t rc_family;
    bdaddr_t rc_bdaddr;
    quint8 rc_channel;
};

bool parseBdaddr(const QString &address, bdaddr_t *out)
{
    const QStringList parts = address.split(QLatin1Char(':'));
    if (parts.size() != 6 || !out)
        return false;
    for (int i = 0; i < 6; ++i) {
        bool ok = false;
        const int v = parts[i].toInt(&ok, 16);
        if (!ok || v < 0 || v > 255)
            return false;
        out->b[5 - i] = quint8(v);
    }
    return true;
}

int resolveRfcommChannel(const QString &address)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(QStringLiteral("sdptool"), {QStringLiteral("browse"), address});
    if (!proc.waitForFinished(15000)) {
        proc.kill();
        proc.waitForFinished(1000);
        return 0;
    }
    const QString out = QString::fromUtf8(proc.readAll());
    const QString uuid = QStringLiteral("00000000-deca-fade-deca-deafdecacafe");
    const QStringList blocks = out.split(QStringLiteral("Service Name:"), Qt::SkipEmptyParts);
    for (const QString &block : blocks) {
        if (!block.contains(uuid, Qt::CaseInsensitive)
            && !block.contains(QStringLiteral("iAP2"), Qt::CaseInsensitive)
            && !block.contains(QStringLiteral("Wireless iAP"), Qt::CaseInsensitive))
            continue;
        const QRegularExpression re(QStringLiteral("Channel:\\s*(\\d+)"));
        const auto m = re.match(block);
        if (m.hasMatch())
            return m.captured(1).toInt();
    }
    const QRegularExpression re(QStringLiteral("Channel:\\s*(\\d+)"));
    const auto m = re.match(out);
    return m.hasMatch() ? m.captured(1).toInt() : 0;
}

} // namespace
#endif

BluetoothRfcomm::BluetoothRfcomm(QObject *parent)
    : QIODevice(parent)
{
}

BluetoothRfcomm::~BluetoothRfcomm()
{
    close();
}

QString BluetoothRfcomm::errorString() const
{
    QMutexLocker lock(&m_mutex);
    return m_error;
}

bool BluetoothRfcomm::isSequential() const
{
    return true;
}

bool BluetoothRfcomm::open(OpenMode mode)
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    QMutexLocker lock(&m_mutex);
    if (m_socket < 0) {
        m_error = QStringLiteral("not connected");
        return false;
    }
    return QIODevice::open(mode | QIODevice::Unbuffered);
#else
    Q_UNUSED(mode);
    QMutexLocker lock(&m_mutex);
    m_error = QStringLiteral("Bluetooth RFCOMM unsupported on this platform");
    return false;
#endif
}

void BluetoothRfcomm::close()
{
    disconnectFromHost();
    QIODevice::close();
}

void BluetoothRfcomm::disconnectFromHost()
{
#ifdef Q_OS_WIN
    QMutexLocker lock(&m_mutex);
    if (m_socket != kInvalidSock) {
        ::closesocket(asSock(m_socket));
        m_socket = kInvalidSock;
    }
    if (m_wsaStarted) {
        WSACleanup();
        m_wsaStarted = false;
    }
#elif defined(Q_OS_LINUX)
    QMutexLocker lock(&m_mutex);
    if (m_socket >= 0) {
        ::close(int(m_socket));
        m_socket = -1;
    }
#endif
}

BluetoothRfcomm::WriteFn BluetoothRfcomm::writeCallback()
{
    return [this](const QByteArray &data) -> qint64 {
        QMutexLocker lock(&m_mutex);
#ifdef Q_OS_WIN
        if (m_socket == kInvalidSock)
            return -1;
        int total = 0;
        while (total < data.size()) {
            const int n = ::send(asSock(m_socket), data.constData() + total, data.size() - total, 0);
            if (n == SOCKET_ERROR || n == 0)
                return -1;
            total += n;
        }
        return total;
#elif defined(Q_OS_LINUX)
        if (m_socket < 0)
            return -1;
        int total = 0;
        while (total < data.size()) {
            const ssize_t n = ::write(int(m_socket), data.constData() + total, size_t(data.size() - total));
            if (n <= 0)
                return -1;
            total += int(n);
        }
        return total;
#else
        Q_UNUSED(data);
        return -1;
#endif
    };
}

BluetoothRfcomm::ReadFn BluetoothRfcomm::readCallback()
{
    return [this](int maxBytes) -> QByteArray {
        if (maxBytes <= 0)
            return {};
        QMutexLocker lock(&m_mutex);
#ifdef Q_OS_WIN
        if (m_socket == kInvalidSock)
            return {};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(asSock(m_socket), &fds);
        timeval tv{};
        const int ready = ::select(0, &fds, nullptr, nullptr, &tv);
        if (ready <= 0)
            return {};
        QByteArray buf(maxBytes, Qt::Uninitialized);
        const int n = ::recv(asSock(m_socket), buf.data(), maxBytes, 0);
        if (n <= 0)
            return {};
        buf.resize(n);
        return buf;
#elif defined(Q_OS_LINUX)
        if (m_socket < 0)
            return {};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(int(m_socket), &fds);
        timeval tv{};
        const int ready = ::select(int(m_socket) + 1, &fds, nullptr, nullptr, &tv);
        if (ready <= 0)
            return {};
        QByteArray buf(maxBytes, Qt::Uninitialized);
        const ssize_t n = ::read(int(m_socket), buf.data(), size_t(maxBytes));
        if (n <= 0)
            return {};
        buf.resize(int(n));
        return buf;
#else
        Q_UNUSED(maxBytes);
        return {};
#endif
    };
}

qint64 BluetoothRfcomm::bytesAvailable() const
{
#ifdef Q_OS_WIN
    QMutexLocker lock(&m_mutex);
    if (m_socket == kInvalidSock)
        return QIODevice::bytesAvailable();
    u_long pending = 0;
    if (ioctlsocket(asSock(m_socket), FIONREAD, &pending) != 0)
        return QIODevice::bytesAvailable();
    return qint64(pending) + QIODevice::bytesAvailable();
#elif defined(Q_OS_LINUX)
    return QIODevice::bytesAvailable();
#else
    return QIODevice::bytesAvailable();
#endif
}

qint64 BluetoothRfcomm::readData(char *data, qint64 maxlen)
{
#ifdef Q_OS_WIN
    if (!data || maxlen <= 0)
        return -1;
    QMutexLocker lock(&m_mutex);
    if (m_socket == kInvalidSock)
        return -1;
    const int n = ::recv(asSock(m_socket), data, int(qMin(maxlen, qint64(0x7fffffff))), 0);
    if (n == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return 0;
        return -1;
    }
    return n;
#elif defined(Q_OS_LINUX)
    if (!data || maxlen <= 0)
        return -1;
    QMutexLocker lock(&m_mutex);
    if (m_socket < 0)
        return -1;
    const ssize_t n = ::read(int(m_socket), data, size_t(qMin(maxlen, qint64(0x7fffffff))));
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        return -1;
    }
    return qint64(n);
#else
    Q_UNUSED(data);
    Q_UNUSED(maxlen);
    return -1;
#endif
}

qint64 BluetoothRfcomm::writeData(const char *data, qint64 len)
{
#ifdef Q_OS_WIN
    if (!data || len <= 0)
        return -1;
    QMutexLocker lock(&m_mutex);
    if (m_socket == kInvalidSock)
        return -1;
    const int n = ::send(asSock(m_socket), data, int(qMin(len, qint64(0x7fffffff))), 0);
    if (n == SOCKET_ERROR)
        return -1;
    return n;
#elif defined(Q_OS_LINUX)
    if (!data || len <= 0)
        return -1;
    QMutexLocker lock(&m_mutex);
    if (m_socket < 0)
        return -1;
    const ssize_t n = ::write(int(m_socket), data, size_t(qMin(len, qint64(0x7fffffff))));
    if (n < 0)
        return -1;
    return qint64(n);
#else
    Q_UNUSED(data);
    Q_UNUSED(len);
    return -1;
#endif
}

bool BluetoothRfcomm::connectTo(const QString &address)
{
#ifdef Q_OS_WIN
    QMutexLocker lock(&m_mutex);
    m_error.clear();
    if (m_socket != kInvalidSock) {
        ::closesocket(asSock(m_socket));
        m_socket = kInvalidSock;
    }

    const QStringList parts = address.split(QLatin1Char(':'));
    if (parts.size() != 6) {
        m_error = QStringLiteral("invalid Bluetooth address");
        return false;
    }
    BTH_ADDR addr = 0;
    for (int i = 0; i < 6; ++i) {
        bool ok = false;
        const int v = parts[i].toInt(&ok, 16);
        if (!ok || v < 0 || v > 255) {
            m_error = QStringLiteral("invalid Bluetooth address");
            return false;
        }
        addr = (addr << 8) | BTH_ADDR(v & 0xff);
    }

    WSADATA wsa{};
    if (!m_wsaStarted) {
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            m_error = QStringLiteral("WSAStartup failed");
            return false;
        }
        m_wsaStarted = true;
    }

    const SOCKET sock = ::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (sock == INVALID_SOCKET) {
        m_error = QStringLiteral("RFCOMM socket failed (%1)").arg(WSAGetLastError());
        return false;
    }

    GUID service = {0x00000000, 0xdeca, 0xfade, {0xde, 0xca, 0xde, 0xaf, 0xde, 0xca, 0xca, 0xfe}};

    SOCKADDR_BTH sa{};
    sa.addressFamily = AF_BTH;
    sa.btAddr = addr;
    sa.serviceClassId = service;
    sa.port = BT_PORT_ANY;

    if (::connect(sock, reinterpret_cast<SOCKADDR *>(&sa), sizeof(sa)) == SOCKET_ERROR) {
        const int err = WSAGetLastError();
        QString hint;
        switch (err) {
        case 10049: hint = QStringLiteral("地址不可用"); break;
        case 10060: hint = QStringLiteral("连接超时"); break;
        case 10061: hint = QStringLiteral("被拒绝"); break;
        case 10013: hint = QStringLiteral("权限不足"); break;
        case 10022: hint = QStringLiteral("参数无效/服务未找到"); break;
        default: hint = QStringLiteral("系统错误"); break;
        }
        m_error = QStringLiteral("RFCOMM 失败 %1（%2），手机需已配对且靠近电脑").arg(err).arg(hint);
        ::closesocket(sock);
        return false;
    }

    u_long nonBlock = 1;
    ioctlsocket(sock, FIONBIO, &nonBlock);
    m_socket = qintptr(sock);
    lock.unlock();
    if (isOpen())
        QIODevice::close();
    if (!QIODevice::open(QIODevice::ReadWrite | QIODevice::Unbuffered)) {
        QMutexLocker lock2(&m_mutex);
        m_error = QStringLiteral("QIODevice open failed");
        ::closesocket(sock);
        m_socket = kInvalidSock;
        return false;
    }
    return true;
#elif defined(Q_OS_LINUX)
    QMutexLocker lock(&m_mutex);
    m_error.clear();
    if (m_socket >= 0) {
        ::close(int(m_socket));
        m_socket = -1;
    }

    bdaddr_t bd{};
    if (!parseBdaddr(address, &bd)) {
        m_error = QStringLiteral("invalid Bluetooth address");
        return false;
    }

    int channel = resolveRfcommChannel(address);
    if (channel <= 0)
        channel = 1;

    const int sock = ::socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (sock < 0) {
        m_error = QStringLiteral("RFCOMM socket failed (%1)").arg(strerror(errno));
        return false;
    }

    sockaddr_rc sa{};
    sa.rc_family = AF_BLUETOOTH;
    sa.rc_bdaddr = bd;
    sa.rc_channel = quint8(channel);

    if (::connect(sock, reinterpret_cast<sockaddr *>(&sa), sizeof(sa)) < 0) {
        m_error = QStringLiteral("RFCOMM 失败 ch=%1 (%2)，手机需已配对").arg(channel).arg(strerror(errno));
        ::close(sock);
        return false;
    }

    const int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0)
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    m_socket = qintptr(sock);
    lock.unlock();
    if (isOpen())
        QIODevice::close();
    if (!QIODevice::open(QIODevice::ReadWrite | QIODevice::Unbuffered)) {
        QMutexLocker lock2(&m_mutex);
        m_error = QStringLiteral("QIODevice open failed");
        ::close(sock);
        m_socket = -1;
        return false;
    }
    return true;
#else
    QMutexLocker lock(&m_mutex);
    m_error = QStringLiteral("Bluetooth RFCOMM unsupported on this platform");
    return false;
#endif
}

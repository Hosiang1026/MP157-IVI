#include "AirPlayMirrorSession.hpp"

#include "ExistingWifi.hpp"

#include <QCryptographicHash>
#include <QHostAddress>
#include <QMap>

namespace {

QString derivedDeviceId(const AirPlayIdentity &identity)
{
    const QByteArray digest = QCryptographicHash::hash(identity.publicKey(), QCryptographicHash::Sha256);
    QByteArray mac = digest.left(6);
    if (mac.size() < 6)
        mac = QByteArray(6, char(0x02));
    mac[0] = char((quint8(mac[0]) & 0xfc) | 0x02);
    return QStringLiteral("%1:%2:%3:%4:%5:%6")
        .arg(quint8(mac[0]), 2, 16, QLatin1Char('0'))
        .arg(quint8(mac[1]), 2, 16, QLatin1Char('0'))
        .arg(quint8(mac[2]), 2, 16, QLatin1Char('0'))
        .arg(quint8(mac[3]), 2, 16, QLatin1Char('0'))
        .arg(quint8(mac[4]), 2, 16, QLatin1Char('0'))
        .arg(quint8(mac[5]), 2, 16, QLatin1Char('0'))
        .toUpper();
}

} // namespace

AirPlayMirrorSession::AirPlayMirrorSession(QObject *parent)
    : QObject(parent)
    , m_status(QStringLiteral("未启动"))
    , m_identity(AirPlayIdentity::loadOrCreate())
{
    m_deviceId = derivedDeviceId(m_identity);
    m_mfi.load(LocalMfiAuth::defaultDirectory());

    connect(&m_airPlay, &AirPlayServer::listening, this, [this](quint16 port) {
        setDetail(QStringLiteral("监听 %1:%2 · 同 Wi‑Fi 从控制中心选 MP157-AirPlay")
                      .arg(m_hostIp)
                      .arg(port));
    });
    connect(&m_airPlay, &AirPlayServer::clientConnected, this, [this](const QString &peer) {
        setStatus(QStringLiteral("已连接"));
        setDetail(peer);
    });
    connect(&m_airPlay, &AirPlayServer::sessionActive, this, [this] {
        setStatus(QStringLiteral("镜像中"));
    });
    connect(&m_airPlay, &AirPlayServer::videoFrameChanged, this, &AirPlayMirrorSession::videoFrameChanged);
    connect(&m_airPlay, &AirPlayServer::failed, this, [this](const QString &msg) {
        setStatus(QStringLiteral("失败"));
        setDetail(msg);
        stop();
    });
    connect(&m_bonjour, &BonjourAdvertiser::failed, this, [this](const QString &msg) {
        setStatus(QStringLiteral("发现失败"));
        setDetail(msg);
        stop();
    });
}

AirPlayMirrorSession::~AirPlayMirrorSession()
{
    stop();
}

void AirPlayMirrorSession::reloadIdentity()
{
    m_identity = AirPlayIdentity::loadOrCreate();
    m_deviceId = derivedDeviceId(m_identity);
    m_mfi.load(LocalMfiAuth::defaultDirectory());
    emit identityReadyChanged();
}

void AirPlayMirrorSession::start()
{
    if (m_running)
        return;

    reloadIdentity();
    m_hostIp = ExistingWifi::primaryIpv4();
    emit hostIpChanged();
    if (m_hostIp.isEmpty()) {
        setStatus(QStringLiteral("无网络"));
        setDetail(QStringLiteral("需要局域网 IPv4"));
        return;
    }
    if (!m_mfi.isReady()) {
        setStatus(QStringLiteral("身份未就绪"));
        setDetail(m_mfi.errorString().isEmpty() ? QStringLiteral("缺少 offline-mfi")
                                                : m_mfi.errorString());
        return;
    }

    m_airPlay.setDisplaySize(1024, 600);
    const QString sourceVersion = QStringLiteral("220.68");
    if (!m_airPlay.start(&m_identity,
                         &m_mfi,
                         QStringLiteral("MP157-AirPlay"),
                         m_deviceId,
                         QStringLiteral("AppleTV3,2"),
                         QHostAddress::AnyIPv4,
                         sourceVersion)) {
        setStatus(QStringLiteral("启动失败"));
        setDetail(QStringLiteral("端口被占用，请先停止 CarPlay"));
        return;
    }

    QMap<QString, QString> txt;
    txt.insert(QStringLiteral("deviceid"), m_deviceId);
    txt.insert(QStringLiteral("features"), QStringLiteral("0x5A7FFFF7,0x1E"));
    txt.insert(QStringLiteral("flags"), QStringLiteral("0x4"));
    txt.insert(QStringLiteral("model"), QStringLiteral("AppleTV3,2"));
    txt.insert(QStringLiteral("srcvers"), sourceVersion);
    txt.insert(QStringLiteral("vv"), QStringLiteral("2"));
    txt.insert(QStringLiteral("pi"), m_identity.pairingId());
    txt.insert(QStringLiteral("pk"), m_identity.publicKeyHex());

    if (!m_bonjour.start(QStringLiteral("MP157-AirPlay"), m_airPlay.port(), txt,
                         QHostAddress(m_hostIp), m_deviceId, sourceVersion)) {
        m_airPlay.stop();
        setStatus(QStringLiteral("Bonjour 失败"));
        setDetail(QStringLiteral("mDNS 5353 被占用，请先停止 CarPlay"));
        return;
    }

    setRunning(true);
    setStatus(QStringLiteral("等待镜像"));
    setDetail(QStringLiteral("%1:%2").arg(m_hostIp).arg(m_airPlay.port()));
}

void AirPlayMirrorSession::stop()
{
    m_bonjour.stop();
    m_airPlay.stop();
    if (!m_running && m_status != QStringLiteral("失败") && m_status != QStringLiteral("发现失败")
        && m_status != QStringLiteral("启动失败") && m_status != QStringLiteral("无网络")
        && m_status != QStringLiteral("身份未就绪")) {
        setStatus(QStringLiteral("已停止"));
    }
    setRunning(false);
    emit videoFrameChanged();
}

void AirPlayMirrorSession::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void AirPlayMirrorSession::setDetail(const QString &detail)
{
    if (m_detail == detail)
        return;
    m_detail = detail;
    emit detailChanged();
}

void AirPlayMirrorSession::setRunning(bool running)
{
    if (m_running == running)
        return;
    m_running = running;
    emit runningChanged();
}

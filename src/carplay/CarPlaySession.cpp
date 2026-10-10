#include "CarPlaySession.hpp"

#include "BluetoothDevices.hpp"
#include "BluetoothRfcomm.hpp"
#include "CallSession.hpp"
#include "NotificationSession.hpp"
#include "ExistingWifi.hpp"
#include "GpsSource.hpp"
#include "Iap2LinkEngine.hpp"
#include "MediaSession.hpp"
#include "NavSession.hpp"
#include "VehicleState.hpp"
#include "Iap2LinkSession.hpp"
#include "Iap2Protocol.hpp"
#include "Iap2WirelessBootstrap.hpp"
#include "WifiAccessPoint.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QMetaObject>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QScreen>
#include <QSettings>
#include <QTextStream>
#include <QThread>
#include <QWindow>

namespace {

QString localBluetoothMacFallback()
{
    const QString bt = BluetoothDevices::localAdapterAddress();
    if (bt.size() == 17)
        return bt;
    return QStringLiteral("02:00:00:00:00:01");
}

QString derivedAirPlayDeviceId(const AirPlayIdentity &identity)
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

QString normalizeMac(QString value)
{
    value = value.trimmed().toUpper().replace(QLatin1Char('-'), QLatin1Char(':'));
    return value;
}

void carPlayLog(const QString &line)
{
    const QString path = QDir(QCoreApplication::applicationDirPath())
                             .filePath(QStringLiteral("carplay.log"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    QTextStream out(&file);
    out << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"))
        << ' ' << line << '\n';
}

} // namespace

CarPlaySession::CarPlaySession(QObject *parent)
    : QObject(parent)
    , m_status(QStringLiteral("未就绪"))
    , m_airPlayIdentity(AirPlayIdentity::loadOrCreate())
{
    m_deviceId = derivedAirPlayDeviceId(m_airPlayIdentity);
    loadSettings();

    m_airPlayWatchdog.setSingleShot(true);
    m_airPlayWatchdog.setInterval(90000);
    connect(&m_airPlayWatchdog, &QTimer::timeout, this, [this] {
        if (!m_running)
            return;
        setStatus(QStringLiteral("连接超时"));
        setDetail(QStringLiteral("手机未连 %1:7000；请关掉局域网其它 AirPlay，手机 Safari 打开 http://%1:7000/")
                      .arg(ExistingWifi::primaryIpv4()));
        carPlayLog(QStringLiteral("airplay first-tcp timeout"));
        stopInternal();
    });
    m_airPlayNudge.setInterval(2500);
    connect(&m_airPlayNudge, &QTimer::timeout, this, [this] {
        if (!m_running || m_airPlayUp.load()) {
            m_airPlayNudge.stop();
            return;
        }
        m_bonjour.browseNow();
    });

    connect(&m_airPlay, &AirPlayServer::listening, this, [this](quint16 port) {
        setDetail(QStringLiteral("AirPlay 监听 %1 · 等待蓝牙握手").arg(port));
        carPlayLog(QStringLiteral("airplay listening %1").arg(port));
    });
    connect(&m_airPlay, &AirPlayServer::clientConnected, this, [this](const QString &peer) {
        m_airPlayUp.store(true);
        m_airPlayWatchdog.stop();
        m_airPlayNudge.stop();
        rememberPhoneIp(peer);
        setStatus(QStringLiteral("AirPlay 已接入"));
        setDetail(QStringLiteral("手机 %1 已发起会话").arg(peer));
        carPlayLog(QStringLiteral("airplay phone session %1").arg(peer));
    });
    connect(&m_airPlay, &AirPlayServer::sessionActive, this, [this] {
        m_airPlayUp.store(true);
        m_airPlayWatchdog.stop();
        setStatus(QStringLiteral("会话激活"));
        setDetail(QStringLiteral("RECORD · CarPlay 会话进行中"));
        carPlayLog(QStringLiteral("airplay session active"));
        m_airPlay.setNightMode(m_nightMode);
        sendLocationNow();
    });
    connect(&m_airPlay, &AirPlayServer::videoFrameChanged, this, &CarPlaySession::videoFrameChanged);
    connect(&m_airPlay, &AirPlayServer::hostUiRequested, this, &CarPlaySession::hostUiRequested);
    connect(&m_airPlay, &AirPlayServer::nowPlayingInfo, this,
            [this](const QString &title, const QString &artist, bool playing, int positionSec,
                   int durationSec) {
                if (m_media)
                    m_media->applyRemoteNowPlaying(title, artist, playing, positionSec, durationSec);
            });
    connect(&m_airPlay, &AirPlayServer::navigationInfo, this,
            [this](bool active, const QString &text, const QString &turn, int speedLimit, int etaMin,
                   const QString &destination) {
                if (m_nav)
                    m_nav->applyRemote(active, text, turn, speedLimit, etaMin, destination);
            });
    connect(&m_airPlay, &AirPlayServer::telephonyInfo, this,
            [this](bool active, bool ringing, const QString &name, const QString &number) {
                if (m_call)
                    m_call->applyRemote(active, ringing, name, number);
            });
    connect(&m_airPlay, &AirPlayServer::notificationInfo, this,
            [this](const QString &appName, const QString &title, const QString &body) {
                if (m_notifications)
                    m_notifications->applyRemote(appName, title, body);
            });
    connect(&m_airPlay, &AirPlayServer::log, this, [this](const QString &msg) {
        if (msg.startsWith(QLatin1String("touch "))
            && !msg.startsWith(QLatin1String("touch dropped")))
            return;
        setDetail(msg);
        carPlayLog(QStringLiteral("airplay %1").arg(msg));
    });
    connect(&m_airPlay, &AirPlayServer::failed, this, [this](const QString &msg) {
        m_airPlayWatchdog.stop();
        setStatus(QStringLiteral("AirPlay 失败"));
        setDetail(msg);
        carPlayLog(QStringLiteral("airplay failed %1").arg(msg));
    });
    connect(&m_bonjour, &BonjourAdvertiser::log, this, [this](const QString &msg) {
        carPlayLog(QStringLiteral("bonjour %1").arg(msg));
    });
    connect(&m_bonjour, &BonjourAdvertiser::ctrlProbed, this, [this](const QString &host, quint16 port,
                                                                   const QString &status) {
        rememberPhoneIp(host);
        setDetail(QStringLiteral("已探测手机 %1:%2 %3").arg(host).arg(port).arg(status));
        carPlayLog(QStringLiteral("ctrl probed %1:%2 %3").arg(host).arg(port).arg(status));
    });
    connect(&m_bonjour, &BonjourAdvertiser::preferredHostInvalid, this, [this](const QString &host) {
        carPlayLog(QStringLiteral("preferred phone ip invalid %1").arg(host));
        if (m_lastPhoneIp == host)
            clearPhoneIp();
    });

    reloadIdentity();
    refreshWifi();
    refreshBluetooth();
    recoverStaleRunning();
}

CarPlaySession::~CarPlaySession()
{
    stopInternal();
    if (m_btThread) {
        m_btThread->disconnect(this);
        m_btThread->requestInterruption();
        m_btThread->wait(5000);
        delete m_btThread;
        m_btThread = nullptr;
    }
}

void CarPlaySession::setGpsSource(GpsSource *gps)
{
    if (m_gps == gps)
        return;
    if (m_gps)
        disconnect(m_gps, nullptr, this, nullptr);
    m_gps = gps;
    if (m_gps)
        connect(m_gps, &GpsSource::positionUpdated, this, &CarPlaySession::onGpsUpdated);
}

void CarPlaySession::setVehicleState(VehicleState *vehicle)
{
    if (m_vehicle == vehicle)
        return;
    if (m_vehicle)
        disconnect(m_vehicle, nullptr, this, nullptr);
    m_vehicle = vehicle;
    if (m_vehicle)
        connect(m_vehicle, &VehicleState::changed, this, &CarPlaySession::onVehicleChanged);
}

void CarPlaySession::setMediaSession(MediaSession *media)
{
    m_media = media;
}

void CarPlaySession::setNavSession(NavSession *nav)
{
    m_nav = nav;
}

void CarPlaySession::setCallSession(CallSession *call)
{
    m_call = call;
}

void CarPlaySession::setNotificationSession(NotificationSession *notifications)
{
    m_notifications = notifications;
}

void CarPlaySession::onGpsUpdated()
{
    if (m_running && m_airPlayUp.load())
        sendLocationNow();
}

void CarPlaySession::onVehicleChanged()
{
    if (m_running && m_airPlayUp.load())
        sendLocationNow();
}

bool CarPlaySession::sendLocationNow()
{
    if (!m_gps || !m_gps->hasFix())
        return false;
    double speedMps = m_gps->speedMps();
    if (m_vehicle && m_vehicle->speed() >= 0)
        speedMps = m_vehicle->speed() / 3.6;
    return m_airPlay.sendLocation(m_gps->latitude(), m_gps->longitude(), m_gps->altitude(),
                                  speedMps, m_gps->course(), m_gps->accuracy());
}

QString CarPlaySession::status() const { return m_status; }
QString CarPlaySession::detail() const { return m_detail; }
bool CarPlaySession::identityReady() const { return m_mfi.isReady(); }
bool CarPlaySession::running() const { return m_running; }

bool CarPlaySession::canStart() const
{
#ifdef Q_OS_LINUX
    const bool wifiOk = m_wifiConnected
        || !qEnvironmentVariableIsSet("IVI_CARPLAY_WIFI_CLIENT");
#else
    const bool wifiOk = m_wifiConnected && !m_wifiPassword.isEmpty();
#endif
    return !m_running
        && !(m_btThread && m_btThread->isRunning())
        && m_mfi.isReady()
        && wifiOk
        && m_bluetoothAddress.size() == 17
        && BluetoothDevices::isPaired(m_bluetoothAddress);
}

bool CarPlaySession::wifiConnected() const { return m_wifiConnected; }
QString CarPlaySession::wifiSsid() const { return m_wifiSsid; }
QString CarPlaySession::wifiPassword() const { return m_wifiPassword; }

void CarPlaySession::setWifiPassword(const QString &password)
{
    if (m_wifiPassword == password)
        return;
    m_wifiPassword = password;
    saveSettings();
    emit wifiChanged();
    emitCanStart();
}

QString CarPlaySession::bluetoothAddress() const { return m_bluetoothAddress; }
QString CarPlaySession::bluetoothName() const { return m_bluetoothName; }
QVariantList CarPlaySession::wifiNetworks() const { return m_wifiNetworks; }
QVariantList CarPlaySession::bluetoothDevices() const { return m_bluetoothDevices; }
QImage CarPlaySession::videoFrame() const { return m_airPlay.videoFrame(); }
bool CarPlaySession::hasVideo() const { return !m_airPlay.videoFrame().isNull(); }
int CarPlaySession::displayWidth() const { return m_airPlay.displayWidth(); }
int CarPlaySession::displayHeight() const { return m_airPlay.displayHeight(); }
int CarPlaySession::safeAreaTop() const { return m_airPlay.safeAreaTop(); }
int CarPlaySession::safeAreaBottom() const { return m_airPlay.safeAreaBottom(); }
int CarPlaySession::safeAreaLeft() const { return m_airPlay.safeAreaLeft(); }
int CarPlaySession::safeAreaRight() const { return m_airPlay.safeAreaRight(); }
bool CarPlaySession::nightMode() const { return m_nightMode; }

void CarPlaySession::setNightMode(bool night)
{
    if (m_nightMode == night)
        return;
    m_nightMode = night;
    saveSettings();
    emit nightModeChanged();
    if (m_running)
        m_airPlay.setNightMode(night);
}

void CarPlaySession::setDisplaySize(int width, int height)
{
    m_airPlay.setDisplaySize(width, height);
    emit displaySizeChanged();
}

void CarPlaySession::setSafeAreaInsets(int top, int bottom, int left, int right)
{
    m_airPlay.setSafeAreaInsets(top, bottom, left, right);
    saveSettings();
    emit safeAreaChanged();
}

void CarPlaySession::sendTouch(double xNorm, double yNorm, bool down)
{
    m_airPlay.sendTouch(xNorm, yNorm, down);
}

bool CarPlaySession::sendHardKey(const QString &key, bool down)
{
    return m_airPlay.sendHardKey(key, down);
}

void CarPlaySession::loadSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("carplay"));
    m_bluetoothAddress = normalizeMac(settings.value(QStringLiteral("bluetoothAddress")).toString());
    m_bluetoothName = settings.value(QStringLiteral("bluetoothName")).toString();
    m_wifiSsid = settings.value(QStringLiteral("wifiSsid")).toString();
    m_wifiPassword = settings.value(QStringLiteral("wifiPassword")).toString();
    m_lastPhoneIp = settings.value(QStringLiteral("lastPhoneIp")).toString().trimmed();
    m_nightMode = settings.value(QStringLiteral("nightMode"), false).toBool();
    m_airPlay.setSafeAreaInsets(settings.value(QStringLiteral("safeTop"), 0).toInt(),
                                settings.value(QStringLiteral("safeBottom"), 0).toInt(),
                                settings.value(QStringLiteral("safeLeft"), 0).toInt(),
                                settings.value(QStringLiteral("safeRight"), 0).toInt());
    settings.endGroup();
}

void CarPlaySession::saveSettings() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("carplay"));
    settings.setValue(QStringLiteral("bluetoothAddress"), m_bluetoothAddress);
    settings.setValue(QStringLiteral("bluetoothName"), m_bluetoothName);
    settings.setValue(QStringLiteral("wifiSsid"), m_wifiSsid);
    settings.setValue(QStringLiteral("wifiPassword"), m_wifiPassword);
    settings.setValue(QStringLiteral("lastPhoneIp"), m_lastPhoneIp);
    settings.setValue(QStringLiteral("nightMode"), m_nightMode);
    settings.setValue(QStringLiteral("safeTop"), m_airPlay.safeAreaTop());
    settings.setValue(QStringLiteral("safeBottom"), m_airPlay.safeAreaBottom());
    settings.setValue(QStringLiteral("safeLeft"), m_airPlay.safeAreaLeft());
    settings.setValue(QStringLiteral("safeRight"), m_airPlay.safeAreaRight());
    settings.endGroup();
}

void CarPlaySession::rememberPhoneIp(const QString &ip)
{
    QString host = ip.trimmed();
    if (host.startsWith(QLatin1Char('['))) {
        const int end = host.indexOf(QLatin1Char(']'));
        host = end > 0 ? host.mid(1, end - 1) : host;
    } else {
        const int colon = host.lastIndexOf(QLatin1Char(':'));
        if (colon > 0 && host.count(QLatin1Char(':')) == 1)
            host = host.left(colon);
    }
    const QHostAddress addr(host);
    if (addr.isNull() || addr.isLoopback() || addr == QHostAddress::AnyIPv4)
        return;
    host = addr.toString();
    if (m_lastPhoneIp == host)
        return;
    m_lastPhoneIp = host;
    m_bonjour.setPreferredHost(host);
    saveSettings();
    carPlayLog(QStringLiteral("remember phone ip %1").arg(host));
}

void CarPlaySession::clearPhoneIp()
{
    if (m_lastPhoneIp.isEmpty())
        return;
    carPlayLog(QStringLiteral("clear phone ip %1").arg(m_lastPhoneIp));
    m_lastPhoneIp.clear();
    m_bonjour.setPreferredHost(QString());
    saveSettings();
}

void CarPlaySession::reloadIdentity()
{
    const bool wasReady = m_mfi.isReady();
    if (m_mfi.load(LocalMfiAuth::defaultDirectory())) {
        setStatus(QStringLiteral("身份就绪"));
        setDetail(QStringLiteral("LOCAL MFi v%1").arg(m_mfi.protocolMajor()));
    } else {
        setStatus(QStringLiteral("身份缺失"));
        setDetail(m_mfi.errorString());
    }
    if (wasReady != m_mfi.isReady())
        emit identityReadyChanged();
}

void CarPlaySession::refreshWifi()
{
    const ExistingWifi wifi = ExistingWifi::query();
    m_wifiConnected = ExistingWifi::isConnected();
    if (m_wifiConnected) {
        if (!wifi.ssid.isEmpty())
            m_wifiSsid = wifi.ssid;
        if (m_wifiPassword.isEmpty()) {
            const QString saved = ExistingWifi::profilePassword(m_wifiSsid);
            if (!saved.isEmpty()) {
                m_wifiPassword = saved;
                saveSettings();
            }
        }
        m_wifiNetworks.clear();
        setDetail(m_wifiPassword.isEmpty()
                      ? QStringLiteral("已连 Wi‑Fi：%1 · %2 · 请填写密码").arg(m_wifiSsid, wifi.ipv4)
                      : QStringLiteral("已连 Wi‑Fi：%1 · %2 · ch%3")
                            .arg(m_wifiSsid, wifi.ipv4)
                            .arg(wifi.channel));
    } else {
        m_wifiNetworks = ExistingWifi::scanNetworks();
        if (m_wifiSsid.isEmpty() && !m_wifiNetworks.isEmpty())
            m_wifiSsid = m_wifiNetworks.first().toMap().value(QStringLiteral("ssid")).toString();
        setDetail(QStringLiteral("未连 Wi‑Fi，请选择网络"));
    }
    emit wifiChanged();
    emit wifiNetworksChanged();
    emitCanStart();
}

void CarPlaySession::refreshBluetooth()
{
    m_bluetoothDevices = BluetoothDevices::listDevices();
    if (!m_bluetoothAddress.isEmpty()) {
        bool stillThere = false;
        for (const QVariant &item : m_bluetoothDevices) {
            const QVariantMap map = item.toMap();
            if (normalizeMac(map.value(QStringLiteral("address")).toString()) != m_bluetoothAddress)
                continue;
            stillThere = true;
            m_bluetoothName = map.value(QStringLiteral("name")).toString();
            if (!map.value(QStringLiteral("paired")).toBool()) {
                m_bluetoothAddress.clear();
                m_bluetoothName.clear();
                saveSettings();
            }
            break;
        }
        if (!stillThere) {
            m_bluetoothAddress.clear();
            m_bluetoothName.clear();
            saveSettings();
        }
    } else {
        for (const QVariant &item : m_bluetoothDevices) {
            const QVariantMap map = item.toMap();
            if (!map.value(QStringLiteral("paired")).toBool())
                continue;
            m_bluetoothAddress = normalizeMac(map.value(QStringLiteral("address")).toString());
            m_bluetoothName = map.value(QStringLiteral("name")).toString();
            saveSettings();
            break;
        }
    }
    emit bluetoothChanged();
    emit bluetoothDevicesChanged();
    emitCanStart();
}

void CarPlaySession::selectWifi(const QString &ssid)
{
    if (m_wifiSsid == ssid)
        return;
    m_wifiSsid = ssid;
    m_wifiPassword.clear();
    emit wifiChanged();
}

bool CarPlaySession::connectWifi(const QString &ssid, const QString &password)
{
    QString error;
    if (!ExistingWifi::connectTo(ssid, password, &error)) {
        setStatus(QStringLiteral("Wi‑Fi 失败"));
        setDetail(error);
        return false;
    }
    m_wifiSsid = ssid;
    m_wifiPassword = password;
    for (int i = 0; i < 20; ++i) {
        QThread::msleep(250);
        if (ExistingWifi::isConnected() && ExistingWifi::currentSsid() == ssid)
            break;
    }
    refreshWifi();
    if (!m_wifiConnected) {
        setStatus(QStringLiteral("Wi‑Fi 失败"));
        setDetail(QStringLiteral("已发起连接，但尚未拿到 IP"));
        return false;
    }
    setStatus(QStringLiteral("Wi‑Fi 已连接"));
    setDetail(m_wifiSsid);
    return true;
}

void CarPlaySession::selectBluetooth(const QString &address, const QString &name)
{
    const QString normalized = normalizeMac(address);
    m_bluetoothAddress = normalized;
    m_bluetoothName = name;
    saveSettings();
    emit bluetoothChanged();
    emitCanStart();
    setStatus(QStringLiteral("已选择手机"));
    setDetail(name.isEmpty() ? normalized : QStringLiteral("%1 · %2").arg(name, normalized));
}

bool CarPlaySession::pairBluetooth(const QString &address)
{
    const QString normalized = normalizeMac(address);
    setStatus(QStringLiteral("等待确认配对码"));
    setDetail(QStringLiteral("请在电脑弹窗和手机上都点「配对」"));

    QString error;
    if (!BluetoothDevices::authenticate(normalized, &error)) {
        setStatus(QStringLiteral("蓝牙配对失败"));
        setDetail(error);
        refreshBluetooth();
        return false;
    }
    if (!BluetoothDevices::isPaired(normalized)) {
        setStatus(QStringLiteral("蓝牙配对失败"));
        setDetail(QStringLiteral("手机未确认配对码"));
        refreshBluetooth();
        return false;
    }

    QString name;
    for (const QVariant &item : BluetoothDevices::listDevices()) {
        const QVariantMap map = item.toMap();
        if (normalizeMac(map.value(QStringLiteral("address")).toString()) == normalized) {
            name = map.value(QStringLiteral("name")).toString();
            break;
        }
    }
    selectBluetooth(normalized, name);
    refreshBluetooth();
    setStatus(QStringLiteral("蓝牙已配对"));
    setDetail(name.isEmpty() ? normalized : name);
    return true;
}

void CarPlaySession::recoverStaleRunning()
{
    if (!m_running)
        return;
    if (m_btThread && m_btThread->isRunning())
        return;
    if (m_btThread && !m_btThread->isRunning()) {
        m_btThread->deleteLater();
        m_btThread = nullptr;
    }
    if (!m_airPlay.isListening()) {
        setRunning(false);
        return;
    }
    stopInternal();
}

void CarPlaySession::emitCanStart()
{
    emit canStartChanged();
}

void CarPlaySession::reconnectLast()
{
    start();
}

void CarPlaySession::start()
{
    recoverStaleRunning();
    if (m_running) {
        setDetail(QStringLiteral("正在连接中，请先点「断开」"));
        return;
    }
    if (m_btThread) {
        if (m_btThread->isRunning()) {
            setDetail(QStringLiteral("正在断开，请稍候"));
            return;
        }
        m_btThread->disconnect(this);
        m_btThread->deleteLater();
        m_btThread = nullptr;
    }
    if (!m_mfi.isReady()) {
        reloadIdentity();
        if (!m_mfi.isReady()) {
            setStatus(QStringLiteral("无法启动"));
            setDetail(m_mfi.errorString());
            emitCanStart();
            return;
        }
    }

    refreshWifi();
    refreshBluetooth();
#ifdef Q_OS_LINUX
    const bool canUseAp = !qEnvironmentVariableIsSet("IVI_CARPLAY_WIFI_CLIENT");
#else
    const bool canUseAp = false;
#endif
    if (!m_wifiConnected && !canUseAp) {
        setStatus(QStringLiteral("需要 Wi‑Fi"));
        setDetail(QStringLiteral("请先选择并连接 Wi‑Fi"));
        emitCanStart();
        return;
    }
    if (m_bluetoothAddress.size() != 17) {
        setStatus(QStringLiteral("需要选择手机"));
        setDetail(QStringLiteral("请先配对并选择 iPhone"));
        emitCanStart();
        return;
    }
    if (!BluetoothDevices::isPaired(m_bluetoothAddress)) {
        setStatus(QStringLiteral("手机未配对"));
        setDetail(QStringLiteral("请先完成配对码确认，再开始 CarPlay"));
        emitCanStart();
        return;
    }

    setRunning(true);
    setStatus(QStringLiteral("启动中"));
    emitCanStart();
    beginWireless();
}

void CarPlaySession::stop()
{
    stopInternal();
    setStatus(m_mfi.isReady() ? QStringLiteral("身份就绪") : QStringLiteral("已停止"));
    setDetail(m_mfi.isReady()
                  ? QStringLiteral("LOCAL MFi v%1").arg(m_mfi.protocolMajor())
                  : m_mfi.errorString());
}

void CarPlaySession::stopInternal()
{
    m_airPlayWatchdog.stop();
    m_airPlayNudge.stop();
    m_airPlayUp.store(true);
    setRunning(false);
    if (m_btThread)
        m_btThread->requestInterruption();
    m_bonjour.stop();
    m_airPlay.stop();
    WifiAccessPoint::instance().stop();
    m_airPlayUp.store(false);
    if (m_media)
        m_media->clearRemoteNowPlaying();
    if (m_nav)
        m_nav->applyRemote(false, {}, {}, 0, 0, {});
    if (m_call)
        m_call->applyRemote(false, false, {}, {});
    emitCanStart();
}

void CarPlaySession::beginWireless()
{
    ExistingWifi wifi = ExistingWifi::query();
#ifdef Q_OS_LINUX
    const bool forceClient = qEnvironmentVariableIsSet("IVI_CARPLAY_WIFI_CLIENT");
    const bool preferAp = !forceClient
        && (qEnvironmentVariableIsSet("IVI_CARPLAY_AP") || !ExistingWifi::isConnected());
    if (preferAp) {
        QString ssid = m_wifiSsid.isEmpty() ? QStringLiteral("MP157-CarPlay") : m_wifiSsid;
        QString pass = m_wifiPassword;
        if (pass.size() < 8)
            pass = QStringLiteral("mp157ivi");
        QString apError;
        setStatus(QStringLiteral("启动热点"));
        setDetail(QStringLiteral("%1 · ch6").arg(ssid));
        if (WifiAccessPoint::instance().start(ssid, pass, 6, &apError)) {
            const auto ap = WifiAccessPoint::instance().info();
            wifi.ssid = ap.ssid;
            wifi.ipv4 = ap.ipv4;
            wifi.mac = ap.mac;
            wifi.channel = ap.channel;
            wifi.bssid = ap.bssid;
            m_wifiSsid = ap.ssid;
            m_wifiPassword = ap.password;
            m_wifiConnected = true;
            saveSettings();
            emit wifiChanged();
            carPlayLog(QStringLiteral("access point up ssid=%1 ip=%2 ch=%3")
                           .arg(ap.ssid, ap.ipv4)
                           .arg(ap.channel));
        } else {
            carPlayLog(QStringLiteral("access point failed: %1").arg(apError));
            if (!ExistingWifi::isConnected()) {
                setStatus(QStringLiteral("热点失败"));
                setDetail(apError);
                setRunning(false);
                return;
            }
        }
    }
#endif

    const QString ipv4 = wifi.ipv4.isEmpty() ? ExistingWifi::primaryIpv4() : wifi.ipv4;
    if (ipv4.isEmpty()) {
        setStatus(QStringLiteral("无 IPv4"));
        setDetail(QStringLiteral("未接入可用局域网"));
        setRunning(false);
        return;
    }

    m_wifiSsid = wifi.ssid.isEmpty() ? m_wifiSsid : wifi.ssid;
    emit wifiChanged();

    if (m_wifiPassword.isEmpty()) {
        setStatus(QStringLiteral("需要 Wi‑Fi 密码"));
        setDetail(QStringLiteral("已连网也要填写同一 Wi‑Fi 密码，才能发给手机"));
        setRunning(false);
        return;
    }
    ExistingWifi wifiInfo = wifi;
    if (wifiInfo.channel <= 0) {
        wifiInfo = ExistingWifi::query();
    }
    if (wifiInfo.channel <= 0 && WifiAccessPoint::instance().isRunning()) {
        wifiInfo.channel = WifiAccessPoint::instance().info().channel;
        wifiInfo.bssid = WifiAccessPoint::instance().info().bssid;
    }
    if (wifiInfo.channel <= 0) {
        setStatus(QStringLiteral("无法读取 Wi‑Fi 信道"));
        setDetail(QStringLiteral("请确认已连 WLAN 后重试（需要 Channel）"));
        carPlayLog(QStringLiteral("abort: wifi channel unresolved ssid=%1").arg(wifiInfo.ssid));
        setRunning(false);
        return;
    }

    m_airPlayUp.store(false);
    m_deviceId = derivedAirPlayDeviceId(m_airPlayIdentity);

    {
        // MP157 7" RGB: 1024x600
        const int targetW = 1024;
        const int targetH = 600;
        setDisplaySize(targetW, targetH);
        carPlayLog(QStringLiteral("display size %1x%2").arg(targetW).arg(targetH));
    }

    const QString sourceVersion = QStringLiteral("950.7.1");
    if (!m_airPlay.start(&m_airPlayIdentity,
                         &m_mfi,
                         QStringLiteral("MP157-IVI"),
                         m_deviceId,
                         QStringLiteral("MP157-IVI"),
                         QHostAddress::AnyIPv4,
                         sourceVersion,
                         localBluetoothMacFallback())) {
        setStatus(QStringLiteral("AirPlay 启动失败"));
        setRunning(false);
        return;
    }

    QMap<QString, QString> txt;
    txt.insert(QStringLiteral("deviceid"), m_deviceId);
    txt.insert(QStringLiteral("features"), QStringLiteral("0x5653aee2,0x61"));
    txt.insert(QStringLiteral("flags"), QStringLiteral("0x4"));
    txt.insert(QStringLiteral("model"), QStringLiteral("MP157-IVI"));
    txt.insert(QStringLiteral("srcvers"), sourceVersion);
    txt.insert(QStringLiteral("protovers"), QStringLiteral("1.1"));
    txt.insert(QStringLiteral("pi"), m_airPlayIdentity.pairingId());
    txt.insert(QStringLiteral("pk"), m_airPlayIdentity.publicKeyHex());

    if (!m_bonjour.start(QStringLiteral("MP157-IVI"), m_airPlay.port(), txt, QHostAddress(ipv4),
                         m_deviceId, sourceVersion)) {
        setStatus(QStringLiteral("Bonjour 失败"));
        m_airPlay.stop();
        setRunning(false);
        return;
    }
    m_bonjour.setPreferredHost(m_lastPhoneIp);
    m_bonjour.browseNow();

    setStatus(QStringLiteral("连接蓝牙"));
    setDetail(m_bluetoothName.isEmpty() ? m_bluetoothAddress
                                        : QStringLiteral("%1 · %2").arg(m_bluetoothName, m_bluetoothAddress));

    const QString ssid = m_wifiSsid;
    const QString pass = m_wifiPassword;
    const QString btAddr = m_bluetoothAddress;
    const QString deviceId = m_deviceId;
    const QString publicKey = m_airPlayIdentity.publicKeyHex();
    const quint16 airPlayPort = m_airPlay.port();
    const QString hostIp = ipv4;
    const quint8 channel = quint8(qBound(0, wifiInfo.channel, 255));
    const QByteArray bssid = wifiInfo.bssid.size() == 6 ? wifiInfo.bssid : wifi.bssid;
    LocalMfiAuth *mfi = &m_mfi;

    carPlayLog(QStringLiteral("begin wireless ssid=%1 bt=%2 ip=%3 port=%4 passLen=%5 ch=%6 bssid=%7 deviceId=%8")
                   .arg(ssid, btAddr, hostIp)
                   .arg(airPlayPort)
                   .arg(pass.size())
                   .arg(channel)
                   .arg(QString::fromLatin1(bssid.toHex(':')), deviceId));

    m_btThread = QThread::create([this, ssid, pass, btAddr, deviceId, publicKey, airPlayPort, hostIp, channel, bssid, mfi] {
        auto report = [this](const QString &status, const QString &detail) {
            carPlayLog(status + QLatin1Char(' ') + detail);
            QMetaObject::invokeMethod(this, [this, status, detail] {
                setStatus(status);
                setDetail(detail);
            }, Qt::QueuedConnection);
        };

        BluetoothRfcomm rfcomm;
        if (!rfcomm.connectTo(btAddr)) {
            report(QStringLiteral("蓝牙失败"), rfcomm.errorString());
            QMetaObject::invokeMethod(this, [this] { stopInternal(); }, Qt::QueuedConnection);
            return;
        }
        report(QStringLiteral("蓝牙已连接"), QStringLiteral("iAP2 协商中"));

        Iap2LinkSession link;
        link.setDevice(&rfcomm, false);
        link.start(false, iap2WirelessLinkConfig());

        Iap2Protocol::WirelessIdentity identity;
        identity.bluetoothMac = localBluetoothMacFallback();
        identity.ssid = ssid;

        Iap2WirelessBootstrap::Endpoint endpoint;
        endpoint.ssid = ssid;
        endpoint.passphrase = pass;
        endpoint.channel = channel;
        endpoint.securityType = 2;
        endpoint.bssid = bssid;
        endpoint.ipAddresses = QStringList{hostIp};
        endpoint.airPlayPort = airPlayPort;
        endpoint.deviceIdentifier = deviceId;
        endpoint.publicKey = publicKey;
        endpoint.sourceVersion = QStringLiteral("950.7.1");

        bool startReported = false;
        Iap2WirelessBootstrap bootstrap;
        const auto result = bootstrap.run(
            link, *mfi, identity, endpoint, 90000,
            [&](const QString &msg) {
                report(QStringLiteral("iAP2"), msg);
                if (!startReported && msg.contains(QStringLiteral("tx=0x4301"))) {
                    startReported = true;
                    report(QStringLiteral("等待 AirPlay"),
                           QStringLiteral("已发 StartSession · 保持蓝牙 · 等 %1:%2")
                               .arg(hostIp)
                               .arg(airPlayPort));
                    QMetaObject::invokeMethod(this, [this] {
                        m_airPlayWatchdog.start();
                        m_airPlayNudge.start();
                        m_bonjour.browseNow();
                    }, Qt::QueuedConnection);
                }
            },
            [this] {
                return m_running && !QThread::currentThread()->isInterruptionRequested();
            });

        carPlayLog(QStringLiteral("iap2 bootstrap done status=%1 airplayUp=%2")
                       .arg(int(result.status))
                       .arg(m_airPlayUp.load()));
        link.stop();
        rfcomm.disconnectFromHost();

        if (QThread::currentThread()->isInterruptionRequested() || !m_running)
            return;
        if (result.status != Iap2WirelessBootstrap::Status::Ok && !m_airPlayUp.load()) {
            report(QStringLiteral("蓝牙握手失败"), result.error);
            QMetaObject::invokeMethod(this, [this] { stopInternal(); }, Qt::QueuedConnection);
        }
    });

    connect(m_btThread, &QThread::finished, this, [this] {
        if (m_btThread) {
            m_btThread->deleteLater();
            m_btThread = nullptr;
        }
        emitCanStart();
    });
    m_btThread->start();
}

void CarPlaySession::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void CarPlaySession::setDetail(const QString &detail)
{
    if (m_detail == detail)
        return;
    m_detail = detail;
    emit detailChanged();
}

void CarPlaySession::setRunning(bool running)
{
    if (m_running == running)
        return;
    m_running = running;
    emit runningChanged();
    emitCanStart();
}

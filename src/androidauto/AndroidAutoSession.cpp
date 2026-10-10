#include "AndroidAutoSession.hpp"

#include "GpsSource.hpp"
#include "VehicleState.hpp"

AndroidAutoSession::AndroidAutoSession(QObject *parent)
    : QObject(parent)
    , m_status(QStringLiteral("未启动"))
    , m_detail(QStringLiteral("有线 USB 互联口 · AOAP"))
{
    m_poll.setInterval(200);
    connect(&m_poll, &QTimer::timeout, this, &AndroidAutoSession::onPoll);
    connect(&m_aoap, &AoapTransport::stateChanged, this, &AndroidAutoSession::aoapStateChanged);
    connect(&m_aoap, &AoapTransport::deviceLabelChanged, this, &AndroidAutoSession::deviceLabelChanged);
    connect(&m_aoap, &AoapTransport::detailChanged, this, [this] {
        if (m_running && !m_demoMode)
            setDetail(m_aoap.detail());
    });
    connect(&m_aoap, &AoapTransport::accessoryReady, this, &AndroidAutoSession::onAoapReady);
    connect(&m_aoap, &AoapTransport::failed, this, &AndroidAutoSession::onAoapFailed);
    connect(&m_aoap, &AoapTransport::bulkActivity, this, [this](int in, int out) {
        if (in > 0)
            m_bytesIn += in;
        if (out > 0)
            m_bytesOut += out;
        emit trafficChanged();
    });
    refreshSensors();
}

void AndroidAutoSession::setGpsSource(GpsSource *gps)
{
    if (m_gps == gps)
        return;
    if (m_gps)
        disconnect(m_gps, nullptr, this, nullptr);
    m_gps = gps;
    if (m_gps)
        connect(m_gps, &GpsSource::positionUpdated, this, &AndroidAutoSession::refreshSensors);
    refreshSensors();
}

void AndroidAutoSession::setVehicleState(VehicleState *vehicle)
{
    if (m_vehicle == vehicle)
        return;
    if (m_vehicle)
        disconnect(m_vehicle, nullptr, this, nullptr);
    m_vehicle = vehicle;
    if (m_vehicle)
        connect(m_vehicle, &VehicleState::changed, this, &AndroidAutoSession::refreshSensors);
    refreshSensors();
}

void AndroidAutoSession::setNightMode(bool night)
{
    if (m_nightMode == night)
        return;
    m_nightMode = night;
    emit nightModeChanged();
    refreshSensors();
}

void AndroidAutoSession::start()
{
    if (m_running)
        return;
    m_demoMode = false;
    m_bytesIn = 0;
    m_bytesOut = 0;
    emit trafficChanged();
    emit hasVideoChanged();
    setHasVideo(false);
    setRunning(true);
    setStatus(QStringLiteral("监听 USB 互联口"));
    setDetail(QStringLiteral("请将手机插入前排 USB 互联口"));
    refreshSensors();
    m_aoap.startWatch();
}

void AndroidAutoSession::startDemo()
{
    if (m_running)
        stop();
    m_bytesIn = 0;
    m_bytesOut = 0;
    emit trafficChanged();
    m_demoMode = true;
    setRunning(true);
    openDemoProjection();
}

void AndroidAutoSession::stop()
{
    if (!m_running && !m_sessionOpen)
        return;
    closeSession();
    m_aoap.stop();
    m_demoMode = false;
    setHasVideo(false);
    setRunning(false);
    setStatus(QStringLiteral("已停止"));
    setDetail(QStringLiteral("有线 USB 互联口 · AOAP"));
    emit hasVideoChanged();
}

bool AndroidAutoSession::sendHardKey(const QString &key, bool down)
{
    if (!m_running || !down)
        return false;
    const QString k = key.trimmed().toLower();
    if (k == QLatin1String("voice") || k == QLatin1String("siri")
        || k == QLatin1String("assistant") || k == QLatin1String("google")) {
        setDetail(QStringLiteral("语音助手（Google）· 传感 %1").arg(m_sensorSummary));
        return true;
    }
    if (k == QLatin1String("home")) {
        setDetail(QStringLiteral("Home · %1").arg(m_sensorSummary));
        return true;
    }
    if (k == QLatin1String("play") || k == QLatin1String("pause")
        || k == QLatin1String("playpause") || k == QLatin1String("next")
        || k == QLatin1String("prev") || k == QLatin1String("previous")) {
        setDetail(QStringLiteral("媒体键 %1 · %2").arg(k, m_sensorSummary));
        return true;
    }
    return false;
}

QVariantMap AndroidAutoSession::sensors() const
{
    return m_sensors;
}

void AndroidAutoSession::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void AndroidAutoSession::setDetail(const QString &detail)
{
    if (m_detail == detail)
        return;
    m_detail = detail;
    emit detailChanged();
}

void AndroidAutoSession::setRunning(bool running)
{
    if (m_running == running)
        return;
    m_running = running;
    emit runningChanged();
}

void AndroidAutoSession::setHasVideo(bool hasVideo)
{
    if (m_hasVideo == hasVideo)
        return;
    m_hasVideo = hasVideo;
    emit hasVideoChanged();
}

void AndroidAutoSession::onAoapReady(const QString &deviceLabel)
{
    setStatus(QStringLiteral("附件就绪"));
    setDetail(deviceLabel);
    openLinkProbe();
}

void AndroidAutoSession::onAoapFailed(const QString &reason)
{
    closeSession();
    setHasVideo(false);
    setStatus(QStringLiteral("失败"));
    setDetail(reason);
    setRunning(false);
}

void AndroidAutoSession::openDemoProjection()
{
    m_sessionOpen = true;
    refreshSensors();
    setStatus(QStringLiteral("投影中"));
    setDetail(QStringLiteral("演示画面 · 传感已注入 · TLS/实机视频未接"));
    setHasVideo(true);
}

void AndroidAutoSession::openLinkProbe()
{
    if (m_sessionOpen)
        return;
    m_sessionOpen = true;
    refreshSensors();
    if (qEnvironmentVariableIsSet("IVI_AA_DEMO") || m_demoMode) {
        openDemoProjection();
        return;
    }
    setStatus(QStringLiteral("链路探测"));
    setDetail(QStringLiteral("批量端点已开 · 等待手机数据（TLS 未实现）"));
    m_poll.start();
}

void AndroidAutoSession::closeSession()
{
    m_poll.stop();
    m_sessionOpen = false;
}

void AndroidAutoSession::onPoll()
{
    if (!m_sessionOpen || !m_aoap.accessoryOpen())
        return;
    const QByteArray chunk = m_aoap.readBulk(4096, 50);
    if (chunk.isEmpty())
        return;
    setStatus(QStringLiteral("收到数据"));
    setDetail(QStringLiteral("IN %1 B · 累计 %2/%3（需 TLS 会话）")
                  .arg(chunk.size())
                  .arg(m_bytesIn)
                  .arg(m_bytesOut));
}

void AndroidAutoSession::refreshSensors()
{
    QVariantMap map;
    map.insert(QStringLiteral("night"), m_nightMode);
    if (m_vehicle) {
        map.insert(QStringLiteral("speedKmh"), m_vehicle->speed());
        map.insert(QStringLiteral("gear"), m_vehicle->gear());
        map.insert(QStringLiteral("odometer"), m_vehicle->odometer());
        map.insert(QStringLiteral("trip"), m_vehicle->trip());
    }
    if (m_gps && m_gps->hasFix()) {
        map.insert(QStringLiteral("latitude"), m_gps->latitude());
        map.insert(QStringLiteral("longitude"), m_gps->longitude());
        map.insert(QStringLiteral("course"), m_gps->course());
        map.insert(QStringLiteral("gpsSpeedMps"), m_gps->speedMps());
        map.insert(QStringLiteral("hasFix"), true);
    } else {
        map.insert(QStringLiteral("hasFix"), false);
    }
    m_sensors = map;

    const int speed = m_vehicle ? m_vehicle->speed() : -1;
    const QString gear = m_vehicle ? m_vehicle->gear() : QStringLiteral("-");
    const QString gps = (m_gps && m_gps->hasFix())
        ? QStringLiteral("%1,%2")
              .arg(m_gps->latitude(), 0, 'f', 4)
              .arg(m_gps->longitude(), 0, 'f', 4)
        : QStringLiteral("无定位");
    m_sensorSummary = QStringLiteral("%1 km/h %2 · %3 · %4")
                          .arg(speed)
                          .arg(gear)
                          .arg(m_nightMode ? QStringLiteral("夜") : QStringLiteral("日"))
                          .arg(gps);
    emit sensorsChanged();
}

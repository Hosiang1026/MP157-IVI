#include "GpsSource.hpp"

#include <QDateTime>
#include <QProcess>
#include <QSettings>
#include <QtMath>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

GpsSource::GpsSource(QObject *parent)
    : QObject(parent)
{
    loadSettings();
    const QString envDev = qEnvironmentVariable("IVI_GPS_DEVICE").trimmed();
    if (!envDev.isEmpty())
        m_device = envDev;
    const QString envBaud = qEnvironmentVariable("IVI_GPS_BAUD").trimmed();
    if (!envBaud.isEmpty())
        m_baudRate = envBaud.toInt();

    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &GpsSource::onTick);
    if (m_enabled)
        openDevice();
    m_timer.start();
}

bool GpsSource::enabled() const { return m_enabled; }
bool GpsSource::hasFix() const { return m_hasFix; }
QString GpsSource::device() const { return m_device; }
int GpsSource::baudRate() const { return m_baudRate; }
double GpsSource::latitude() const { return m_lat; }
double GpsSource::longitude() const { return m_lon; }
double GpsSource::altitude() const { return m_alt; }
double GpsSource::speedMps() const { return m_speedMps; }
double GpsSource::course() const { return m_course; }
double GpsSource::accuracy() const { return m_accuracy; }
QString GpsSource::status() const { return m_status; }
bool GpsSource::demoMode() const { return m_demoMode; }

void GpsSource::setEnabled(bool value)
{
    if (m_enabled == value)
        return;
    m_enabled = value;
    saveSettings();
    if (m_enabled)
        openDevice();
    else {
        closeDevice();
        m_status = QStringLiteral("未启用");
    }
    emit changed();
}

void GpsSource::setDevice(const QString &device)
{
    if (m_device == device)
        return;
    m_device = device.trimmed();
    saveSettings();
    if (m_enabled) {
        closeDevice();
        openDevice();
    }
    emit changed();
}

void GpsSource::setBaudRate(int baud)
{
    if (baud <= 0 || m_baudRate == baud)
        return;
    m_baudRate = baud;
    saveSettings();
    if (m_enabled) {
        closeDevice();
        openDevice();
    }
    emit changed();
}

void GpsSource::setDemoMode(bool value)
{
    if (m_demoMode == value)
        return;
    m_demoMode = value;
    saveSettings();
    if (m_demoMode) {
        m_hasFix = true;
        m_status = QStringLiteral("演示定位");
        m_accuracy = 25.0;
        emit positionUpdated();
    }
    emit changed();
}

void GpsSource::setDemoPosition(double lat, double lon)
{
    m_lat = lat;
    m_lon = lon;
    m_hasFix = true;
    if (m_demoMode)
        m_status = QStringLiteral("演示定位");
    saveSettings();
    emit changed();
    emit positionUpdated();
}

void GpsSource::refresh()
{
    if (m_enabled && !m_port.isOpen())
        openDevice();
    emit changed();
    if (m_hasFix)
        emit positionUpdated();
}

void GpsSource::loadSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("gps"));
    m_enabled = settings.value(QStringLiteral("enabled"), false).toBool();
    m_demoMode = settings.value(QStringLiteral("demoMode"), true).toBool();
#ifdef Q_OS_LINUX
    m_device = settings.value(QStringLiteral("device"), QStringLiteral("/dev/ttyAMA0")).toString();
#else
    m_device = settings.value(QStringLiteral("device"), QStringLiteral("COM3")).toString();
#endif
    m_baudRate = settings.value(QStringLiteral("baudRate"), 9600).toInt();
    m_lat = settings.value(QStringLiteral("lat"), 30.2741).toDouble();
    m_lon = settings.value(QStringLiteral("lon"), 120.1551).toDouble();
    settings.endGroup();
    if (m_demoMode) {
        m_hasFix = true;
        m_status = QStringLiteral("演示定位");
    }
}

void GpsSource::saveSettings() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("gps"));
    settings.setValue(QStringLiteral("enabled"), m_enabled);
    settings.setValue(QStringLiteral("demoMode"), m_demoMode);
    settings.setValue(QStringLiteral("device"), m_device);
    settings.setValue(QStringLiteral("baudRate"), m_baudRate);
    settings.setValue(QStringLiteral("lat"), m_lat);
    settings.setValue(QStringLiteral("lon"), m_lon);
    settings.endGroup();
}

void GpsSource::configureBaud() const
{
#ifdef Q_OS_LINUX
    if (m_device.isEmpty())
        return;
    QProcess::execute(QStringLiteral("stty"),
                      {QStringLiteral("-F"), m_device, QString::number(m_baudRate),
                       QStringLiteral("cs8"), QStringLiteral("-cstopb"), QStringLiteral("-parenb"),
                       QStringLiteral("raw"), QStringLiteral("-echo")});
#elif defined(Q_OS_WIN)
    QString path = m_device;
    if (!path.startsWith(QStringLiteral("\\\\.\\")))
        path = QStringLiteral("\\\\.\\") + path;
    HANDLE h = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ, 0, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return;
    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (GetCommState(h, &dcb)) {
        dcb.BaudRate = DWORD(m_baudRate);
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        SetCommState(h, &dcb);
    }
    CloseHandle(h);
#endif
}

void GpsSource::openDevice()
{
    closeDevice();
    if (m_device.isEmpty()) {
        m_status = QStringLiteral("未配置串口");
        emit changed();
        return;
    }
    configureBaud();
#ifdef Q_OS_WIN
    QString path = m_device;
    if (!path.startsWith(QStringLiteral("\\\\.\\")))
        path = QStringLiteral("\\\\.\\") + path;
    m_port.setFileName(path);
#else
    m_port.setFileName(m_device);
#endif
    if (!m_port.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
        m_status = QStringLiteral("打开失败: %1").arg(m_port.errorString());
        if (m_demoMode) {
            m_hasFix = true;
            m_status = QStringLiteral("串口失败 · 演示定位");
        }
        emit changed();
        return;
    }
    m_status = QStringLiteral("等待 NMEA");
    emit changed();
}

void GpsSource::closeDevice()
{
    if (m_port.isOpen())
        m_port.close();
    m_buf.clear();
}

void GpsSource::pollDevice()
{
    if (!m_port.isOpen())
        return;
    const QByteArray chunk = m_port.read(1024);
    if (chunk.isEmpty())
        return;
    m_buf.append(chunk);
    while (true) {
        const int nl = m_buf.indexOf('\n');
        if (nl < 0)
            break;
        QByteArray line = m_buf.left(nl);
        m_buf.remove(0, nl + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        parseNmeaLine(QString::fromLatin1(line));
    }
    if (m_buf.size() > 4096)
        m_buf.clear();
}

void GpsSource::onTick()
{
    pollDevice();
    if (m_demoMode && !m_port.isOpen()) {
        const double t = QDateTime::currentMSecsSinceEpoch() / 1000.0;
        m_course = std::fmod(t * 3.0, 360.0);
        m_speedMps = 8.0 + 2.0 * qSin(t / 17.0);
        m_lat += (m_speedMps * qCos(qDegreesToRadians(m_course))) / 111320.0;
        m_lon += (m_speedMps * qSin(qDegreesToRadians(m_course)))
                 / (111320.0 * qMax(0.2, qCos(qDegreesToRadians(m_lat))));
        m_hasFix = true;
        emit changed();
        emit positionUpdated();
        return;
    }
    if (m_hasFix)
        emit positionUpdated();
}

bool GpsSource::parseLatLon(const QString &lat, const QString &ns, const QString &lon, const QString &ew,
                            double *outLat, double *outLon)
{
    if (lat.size() < 4 || lon.size() < 5 || !outLat || !outLon)
        return false;
    const double latRaw = lat.toDouble();
    const double lonRaw = lon.toDouble();
    const int latDeg = int(latRaw / 100.0);
    const int lonDeg = int(lonRaw / 100.0);
    double la = latDeg + (latRaw - latDeg * 100.0) / 60.0;
    double lo = lonDeg + (lonRaw - lonDeg * 100.0) / 60.0;
    if (ns.compare(QStringLiteral("S"), Qt::CaseInsensitive) == 0)
        la = -la;
    if (ew.compare(QStringLiteral("W"), Qt::CaseInsensitive) == 0)
        lo = -lo;
    if (la < -90.0 || la > 90.0 || lo < -180.0 || lo > 180.0)
        return false;
    *outLat = la;
    *outLon = lo;
    return true;
}

void GpsSource::applyFix(double lat, double lon, double alt, double speedKnots, double course, bool valid)
{
    if (!valid) {
        if (!m_demoMode) {
            m_hasFix = false;
            m_status = QStringLiteral("无定位");
            emit changed();
        }
        return;
    }
    m_lat = lat;
    m_lon = lon;
    if (alt > -1000.0)
        m_alt = alt;
    if (speedKnots >= 0.0)
        m_speedMps = speedKnots * 0.514444;
    if (course >= 0.0)
        m_course = course;
    m_accuracy = 8.0;
    m_hasFix = true;
    m_status = QStringLiteral("已定位");
    emit changed();
    emit positionUpdated();
}

void GpsSource::parseNmeaLine(const QString &line)
{
    if (!line.startsWith(QLatin1Char('$')))
        return;
    const int star = line.indexOf(QLatin1Char('*'));
    const QString body = star > 0 ? line.left(star) : line;
    const QStringList f = body.split(QLatin1Char(','));
    if (f.isEmpty())
        return;
    const QString type = f[0];
    if (type.endsWith(QStringLiteral("RMC")) && f.size() >= 10) {
        const bool valid = f.value(2) == QLatin1String("A");
        double lat = 0;
        double lon = 0;
        if (!parseLatLon(f.value(3), f.value(4), f.value(5), f.value(6), &lat, &lon))
            return;
        applyFix(lat, lon, m_alt, f.value(7).toDouble(), f.value(8).toDouble(), valid);
        return;
    }
    if (type.endsWith(QStringLiteral("GGA")) && f.size() >= 10) {
        const int quality = f.value(6).toInt();
        double lat = 0;
        double lon = 0;
        if (!parseLatLon(f.value(2), f.value(3), f.value(4), f.value(5), &lat, &lon))
            return;
        applyFix(lat, lon, f.value(9).toDouble(), -1.0, -1.0, quality > 0);
    }
}

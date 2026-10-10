#include "AirPlayMirrorSession.hpp"
#include "AirPlayVideoItem.hpp"
#include "AppCatalog.hpp"
#include "AudioFocus.hpp"
#include "CallSession.hpp"
#include "CameraService.hpp"
#include "CameraVideoItem.hpp"
#include "AndroidAutoSession.hpp"
#include "BluetoothMediaHub.hpp"
#include "CarPlaySession.hpp"
#include "CarPlayVideoItem.hpp"
#include "DlnaRenderer.hpp"
#include "DlnaVideoItem.hpp"
#include "MapTiles.hpp"
#include "MediaSession.hpp"
#include "NavSession.hpp"
#include "FileBrowser.hpp"
#include "GpsSource.hpp"
#include "RadioSession.hpp"
#include "SystemState.hpp"
#include "UpdateService.hpp"
#include "VehicleState.hpp"
#include "VideoScreen.hpp"
#include "WallpaperStore.hpp"
#include "WeatherService.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>
#include <QQuickStyle>
#include <QTimer>

#include <cstdio>
#include <cstring>

#if defined(Q_OS_LINUX)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace {

QFile *g_logFile = nullptr;

void iviMessageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    const QByteArray line = qFormatLogMessage(type, ctx, msg).toUtf8() + '\n';
    fwrite(line.constData(), 1, size_t(line.size()), stderr);
    fflush(stderr);
    if (g_logFile && g_logFile->isOpen()) {
        g_logFile->write(line);
        g_logFile->flush();
    }
}

void setupLogging()
{
    qSetMessagePattern(QStringLiteral("%{time yyyy-MM-dd hh:mm:ss.zzz} [%{type}] %{message}"));
    QString dir = qEnvironmentVariable("IVI_LOG_DIR");
    if (dir.isEmpty())
        dir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("logs"));
    QDir().mkpath(dir);
    const QString path = QDir(dir).filePath(QStringLiteral("ivi-shell.log"));
    const QFileInfo info(path);
    if (info.exists() && info.size() > 5 * 1024 * 1024) {
        QFile::remove(path + QStringLiteral(".1"));
        QFile::rename(path, path + QStringLiteral(".1"));
    }
    g_logFile = new QFile(path);
    if (g_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        qInstallMessageHandler(iviMessageHandler);
}

#if defined(Q_OS_LINUX)
bool systemdNotify(const char *state)
{
    const QByteArray path = qgetenv("NOTIFY_SOCKET");
    if (path.isEmpty())
        return false;

    const int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return false;

    sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    socklen_t addrLen = 0;
    if (path.startsWith('@')) {
        addr.sun_path[0] = '\0';
        const int n = qMin(path.size() - 1, int(sizeof(addr.sun_path) - 2));
        memcpy(addr.sun_path + 1, path.constData() + 1, size_t(n));
        addrLen = socklen_t(offsetof(sockaddr_un, sun_path) + 1 + n);
    } else {
        const int n = qMin(path.size(), int(sizeof(addr.sun_path) - 1));
        memcpy(addr.sun_path, path.constData(), size_t(n));
        addr.sun_path[n] = '\0';
        addrLen = socklen_t(offsetof(sockaddr_un, sun_path) + n + 1);
    }

    const size_t len = strlen(state);
    const bool ok = ::sendto(fd, state, len, 0, reinterpret_cast<sockaddr *>(&addr), addrLen) >= 0;
    ::close(fd);
    return ok;
}

void setupSystemdWatchdog(QObject *parent)
{
    systemdNotify("READY=1\n");
    const qint64 usec = qEnvironmentVariableIntValue("WATCHDOG_USEC");
    if (usec <= 0)
        return;
    auto *timer = new QTimer(parent);
    QObject::connect(timer, &QTimer::timeout, parent, [] { systemdNotify("WATCHDOG=1\n"); });
    timer->start(qMax(1000, int(usec / 2000)));
}
#endif


class CarPlayFrameProvider final : public QQuickImageProvider {
public:
    explicit CarPlayFrameProvider(CarPlaySession *session)
        : QQuickImageProvider(QQmlImageProviderBase::Image)
        , m_session(session)
    {
    }

    QImage requestImage(const QString &, QSize *size, const QSize &) override
    {
        const QImage img = m_session ? m_session->videoFrame() : QImage();
        if (size)
            *size = img.size();
        return img;
    }

private:
    CarPlaySession *m_session = nullptr;
};

class DlnaFrameProvider final : public QQuickImageProvider {
public:
    explicit DlnaFrameProvider(DlnaRenderer *renderer)
        : QQuickImageProvider(QQmlImageProviderBase::Image)
        , m_renderer(renderer)
    {
    }

    QImage requestImage(const QString &, QSize *size, const QSize &) override
    {
        const QImage img = m_renderer ? m_renderer->currentImage() : QImage();
        if (size)
            *size = img.size();
        return img;
    }

private:
    DlnaRenderer *m_renderer = nullptr;
};

}

#if defined(Q_OS_LINUX)
static void prepareLinuxDisplayEnv()
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_EGLFS_PHYSICAL_WIDTH"))
        qputenv("QT_QPA_EGLFS_PHYSICAL_WIDTH", "154");
    if (qEnvironmentVariableIsEmpty("QT_QPA_EGLFS_PHYSICAL_HEIGHT"))
        qputenv("QT_QPA_EGLFS_PHYSICAL_HEIGHT", "87");
    if (qEnvironmentVariableIsEmpty("QT_QPA_FB_FORCE_FULLSCREEN"))
        qputenv("QT_QPA_FB_FORCE_FULLSCREEN", "1");
}
#endif

int main(int argc, char *argv[])
{
#if defined(Q_OS_LINUX)
    prepareLinuxDisplayEnv();
#endif
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ivi-shell"));
    QCoreApplication::setOrganizationName(QStringLiteral("MP157"));
    setupLogging();
#ifdef Q_OS_WIN
    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI")));
#else
    {
        QFont font(QStringLiteral("Noto Sans CJK SC"));
        if (!QFontInfo(font).exactMatch())
            font = QFont(QStringLiteral("Noto Sans CJK"));
        if (!QFontInfo(font).exactMatch())
            font = QFont(QStringLiteral("DejaVu Sans"));
        app.setFont(font);
    }
#endif
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    SystemState system;
    UpdateService updater;
    AudioFocus audio;
    WeatherService weather;
    VehicleState vehicle(&weather);
    CameraService camera(&vehicle);
    MediaSession media(&system, &audio);
    BluetoothMediaHub btMedia;
    QObject::connect(&btMedia, &BluetoothMediaHub::activeChanged, &media, [&] {
        media.setBluetoothSource(btMedia.activeName(), !btMedia.activeAddress().isEmpty());
    });
    media.setBluetoothSource(btMedia.activeName(), !btMedia.activeAddress().isEmpty());
    NavSession nav;
    MapTiles mapTiles;
    CallSession call(&audio);
    RadioSession radio(&audio, &media);
    QObject::connect(&media, &MediaSession::playingChanged, &radio, [&media, &radio] {
        if (media.playing() && radio.playing())
            radio.stop();
    });
    FileBrowser files(&media);
    GpsSource gps;
    weather.setGpsSource(&gps);
    AppCatalog catalog;
    WallpaperStore wallpapers;
    CarPlaySession carPlay;
    carPlay.setGpsSource(&gps);
    carPlay.setMediaSession(&media);
    carPlay.setNavSession(&nav);
    carPlay.setCallSession(&call);
    AndroidAutoSession androidAuto;
    QObject::connect(&carPlay, &CarPlaySession::runningChanged, &androidAuto, [&] {
        if (carPlay.running() && androidAuto.running())
            androidAuto.stop();
    });
    QObject::connect(&androidAuto, &AndroidAutoSession::runningChanged, &carPlay, [&] {
        if (androidAuto.running() && carPlay.running())
            carPlay.stop();
    });
    AirPlayMirrorSession airPlayMirror;
    DlnaRenderer dlna(&audio);

    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "SystemState", &system);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "UpdateService", &updater);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "AudioFocus", &audio);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "VehicleState", &vehicle);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "CameraService", &camera);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "MediaSession", &media);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "BluetoothMediaHub", &btMedia);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "NavSession", &nav);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "MapTiles", &mapTiles);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "CallSession", &call);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "RadioSession", &radio);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "FileBrowser", &files);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "GpsSource", &gps);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "AppCatalog", &catalog);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "WallpaperStore", &wallpapers);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "Weather", &weather);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "CarPlaySession", &carPlay);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "AndroidAutoSession", &androidAuto);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "AirPlayMirror", &airPlayMirror);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "DlnaRenderer", &dlna);
    qmlRegisterType<VideoScreen>("Ivi.Services", 1, 0, "VideoScreen");
    qmlRegisterType<CarPlayVideoItem>("Ivi.Services", 1, 0, "CarPlayVideoItem");
    qmlRegisterType<AirPlayVideoItem>("Ivi.Services", 1, 0, "AirPlayVideoItem");
    qmlRegisterType<DlnaVideoItem>("Ivi.Services", 1, 0, "DlnaVideoItem");
    qmlRegisterType<CameraVideoItem>("Ivi.Services", 1, 0, "CameraVideoItem");

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("carplay"), new CarPlayFrameProvider(&carPlay));
    engine.addImageProvider(QStringLiteral("dlna"), new DlnaFrameProvider(&dlna));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
        QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.loadFromModule("IviShell", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;
#if defined(Q_OS_LINUX)
    setupSystemdWatchdog(&app);
#endif
    return app.exec();
}

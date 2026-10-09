#include "AirPlayMirrorSession.hpp"
#include "AirPlayVideoItem.hpp"
#include "AppCatalog.hpp"
#include "AudioFocus.hpp"
#include "CallSession.hpp"
#include "CameraService.hpp"
#include "CameraVideoItem.hpp"
#include "CarPlaySession.hpp"
#include "CarPlayVideoItem.hpp"
#include "DlnaRenderer.hpp"
#include "DlnaVideoItem.hpp"
#include "MapTiles.hpp"
#include "MediaSession.hpp"
#include "NavSession.hpp"
#include "SystemState.hpp"
#include "UpdateService.hpp"
#include "VehicleState.hpp"
#include "VideoScreen.hpp"
#include "WallpaperStore.hpp"
#include "WeatherService.hpp"

#include <QFont>
#include <QFontInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>
#include <QQuickStyle>

namespace {

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
    QCoreApplication::setApplicationName(QStringLiteral("ivi-shell"));
    QCoreApplication::setOrganizationName(QStringLiteral("MP157"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    SystemState system;
    UpdateService updater;
    AudioFocus audio;
    WeatherService weather;
    VehicleState vehicle(&weather);
    CameraService camera(&vehicle);
    MediaSession media(&system, &audio);
    NavSession nav;
    MapTiles mapTiles;
    CallSession call(&audio);
    AppCatalog catalog;
    WallpaperStore wallpapers;
    CarPlaySession carPlay;
    AirPlayMirrorSession airPlayMirror;
    DlnaRenderer dlna(&audio);

    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "SystemState", &system);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "UpdateService", &updater);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "AudioFocus", &audio);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "VehicleState", &vehicle);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "CameraService", &camera);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "MediaSession", &media);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "NavSession", &nav);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "MapTiles", &mapTiles);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "CallSession", &call);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "AppCatalog", &catalog);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "WallpaperStore", &wallpapers);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "Weather", &weather);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "CarPlaySession", &carPlay);
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
    return app.exec();
}

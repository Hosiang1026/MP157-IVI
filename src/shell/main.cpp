#include "AppCatalog.hpp"
#include "AudioFocus.hpp"
#include "CallSession.hpp"
#include "CarPlaySession.hpp"
#include "CarPlayVideoItem.hpp"
#include "MediaSession.hpp"
#include "NavSession.hpp"
#include "SystemState.hpp"
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
    AudioFocus audio;
    VehicleState vehicle;
    MediaSession media(&system, &audio);
    NavSession nav;
    CallSession call(&audio);
    AppCatalog catalog;
    WallpaperStore wallpapers;
    WeatherService weather;
    CarPlaySession carPlay;

    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "SystemState", &system);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "AudioFocus", &audio);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "VehicleState", &vehicle);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "MediaSession", &media);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "NavSession", &nav);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "CallSession", &call);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "AppCatalog", &catalog);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "WallpaperStore", &wallpapers);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "Weather", &weather);
    qmlRegisterSingletonInstance("Ivi.Services", 1, 0, "CarPlaySession", &carPlay);
    qmlRegisterType<VideoScreen>("Ivi.Services", 1, 0, "VideoScreen");
    qmlRegisterType<CarPlayVideoItem>("Ivi.Services", 1, 0, "CarPlayVideoItem");

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("carplay"), new CarPlayFrameProvider(&carPlay));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
        QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.loadFromModule("IviShell", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;
    return app.exec();
}

#include "MapTiles.hpp"

#include <cmath>

#include <QtMath>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>

namespace {
constexpr int kTileSize = 256;
constexpr int kMinZoom = 12;
constexpr int kMaxZoom = 14;
constexpr char kDefaultRemote[] =
    "http://127.0.0.1:1999/styles/basic-preview/{z}/{x}/{y}.png";
}

MapTiles::MapTiles(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
    m_tileRoot = detectTileRoot();
    m_hasLocal = QDir(m_tileRoot).exists();
    const QByteArray env = qgetenv("IVI_TILE_URL");
    m_remoteTemplate = env.isEmpty() ? QString::fromLatin1(kDefaultRemote)
                                     : QString::fromUtf8(env).trimmed();
    rebuildMarkers();
    probeRemote();
}

QString MapTiles::tileRoot() const
{
    return m_tileRoot;
}

QString MapTiles::tileSource() const
{
    if (m_useRemote)
        return QStringLiteral("TileServer GL");
    if (m_hasLocal)
        return QStringLiteral("本地瓦片");
    return QStringLiteral("无底图");
}

bool MapTiles::useRemote() const
{
    return m_useRemote;
}

int MapTiles::tileSize() const
{
    return kTileSize;
}

int MapTiles::zoom() const
{
    return m_zoom;
}

int MapTiles::minZoom() const
{
    return kMinZoom;
}

int MapTiles::maxZoom() const
{
    return kMaxZoom;
}

double MapTiles::centerLat() const
{
    return m_centerLat;
}

double MapTiles::centerLon() const
{
    return m_centerLon;
}

double MapTiles::vehicleLat() const
{
    return m_vehicleLat;
}

double MapTiles::vehicleLon() const
{
    return m_vehicleLon;
}

QVariantList MapTiles::markers() const
{
    return m_markers;
}

bool MapTiles::hasTiles() const
{
    return m_useRemote || m_hasLocal;
}

QString MapTiles::destinationName() const
{
    return m_hasSearch ? m_searchName : QString();
}

double MapTiles::destinationLat() const
{
    return m_searchLat;
}

double MapTiles::destinationLon() const
{
    return m_searchLon;
}

bool MapTiles::hasDestination() const
{
    return m_hasSearch;
}

QVariantList MapTiles::searchResults() const
{
    return m_searchResults;
}

bool MapTiles::searching() const
{
    return m_searching;
}

void MapTiles::setZoom(int z)
{
    z = qBound(kMinZoom, z, kMaxZoom);
    if (z == m_zoom)
        return;
    m_zoom = z;
    emit viewChanged();
}

void MapTiles::setCenter(double lat, double lon)
{
    lat = clampLat(lat);
    lon = wrapLon(lon);
    if (qFuzzyCompare(lat, m_centerLat) && qFuzzyCompare(lon, m_centerLon))
        return;
    m_centerLat = lat;
    m_centerLon = lon;
    emit viewChanged();
}

void MapTiles::panByPixels(double dx, double dy)
{
    const QPointF world = latLonToWorld(m_centerLat, m_centerLon, m_zoom);
    const QPointF next = worldToLatLon(world.x() - dx, world.y() - dy, m_zoom);
    setCenter(next.x(), next.y());
}

void MapTiles::zoomIn()
{
    setZoom(m_zoom + 1);
}

void MapTiles::zoomOut()
{
    setZoom(m_zoom - 1);
}

void MapTiles::centerOnVehicle()
{
    setCenter(m_vehicleLat, m_vehicleLon);
}

void MapTiles::setVehiclePosition(double lat, double lon)
{
    lat = clampLat(lat);
    lon = wrapLon(lon);
    if (qFuzzyCompare(lat, m_vehicleLat) && qFuzzyCompare(lon, m_vehicleLon))
        return;
    m_vehicleLat = lat;
    m_vehicleLon = lon;
    rebuildMarkers();
    emit vehicleChanged();
}

void MapTiles::searchPlaces(const QString &query)
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        clearSearch();
        return;
    }

    setSearching(true);
    QUrl url(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("name"), trimmed);
    q.addQueryItem(QStringLiteral("count"), QStringLiteral("8"));
    q.addQueryItem(QStringLiteral("language"), QStringLiteral("zh"));
    q.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mp157-ivi"));
    req.setTransferTimeout(8000);
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        QVariantList results;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonArray arr = root.value(QStringLiteral("results")).toArray();
            for (const QJsonValue &item : arr) {
                const QJsonObject obj = item.toObject();
                QString name = obj.value(QStringLiteral("name")).toString().trimmed();
                if (name.isEmpty())
                    continue;
                const QString admin = obj.value(QStringLiteral("admin1")).toString().trimmed();
                const QString country = obj.value(QStringLiteral("country")).toString().trimmed();
                QString label = name;
                if (!admin.isEmpty() && admin != name)
                    label += QStringLiteral(" · ") + admin;
                else if (!country.isEmpty())
                    label += QStringLiteral(" · ") + country;
                QVariantMap map;
                map.insert(QStringLiteral("name"), name);
                map.insert(QStringLiteral("label"), label);
                map.insert(QStringLiteral("lat"), obj.value(QStringLiteral("latitude")).toDouble());
                map.insert(QStringLiteral("lon"), obj.value(QStringLiteral("longitude")).toDouble());
                results.append(map);
            }
        }
        m_searchResults = results;
        setSearching(false);
        emit searchResultsChanged();
    });
}

void MapTiles::clearSearch()
{
    if (m_searchResults.isEmpty())
        return;
    m_searchResults.clear();
    emit searchResultsChanged();
}

void MapTiles::goToPlace(const QString &name, double lat, double lon)
{
    m_searchName = name.trimmed().isEmpty() ? QStringLiteral("地点") : name.trimmed();
    m_searchLat = clampLat(lat);
    m_searchLon = wrapLon(lon);
    m_hasSearch = true;
    setZoom(kMaxZoom);
    setCenter(m_searchLat, m_searchLon);
    rebuildMarkers();
    clearSearch();
    emit destinationChanged();
}

void MapTiles::stepTowardDestination(qreal factor)
{
    if (!m_hasSearch)
        return;
    factor = qBound(0.01, factor, 1.0);
    const double lat = m_vehicleLat + (m_searchLat - m_vehicleLat) * factor;
    const double lon = m_vehicleLon + (m_searchLon - m_vehicleLon) * factor;
    setVehiclePosition(lat, lon);
    setCenter(lat, lon);
}

void MapTiles::refreshTileSource()
{
    probeRemote();
}

void MapTiles::setSearching(bool on)
{
    if (m_searching == on)
        return;
    m_searching = on;
    emit searchingChanged();
}

void MapTiles::probeRemote()
{
    if (m_remoteTemplate.isEmpty())
        return;

    const QUrl url(formatTemplate(m_remoteTemplate, 14, 13659, 6745));
    if (!url.isValid() || url.scheme().isEmpty())
        return;

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mp157-ivi"));
    req.setTransferTimeout(2500);
    QNetworkReply *reply = m_net->head(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const bool ok = reply->error() == QNetworkReply::NoError
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
        if (ok == m_useRemote)
            return;
        m_useRemote = ok;
        emit tileSourceChanged();
        emit viewChanged();
    });
}

QString MapTiles::tileUrl(int z, int x, int y) const
{
    if (m_useRemote && !m_remoteTemplate.isEmpty())
        return formatTemplate(m_remoteTemplate, z, x, y);

    const QString path = tileFilePath(z, x, y);
    if (!QFileInfo::exists(path))
        return {};
    return QUrl::fromLocalFile(path).toString();
}

bool MapTiles::tileExists(int z, int x, int y) const
{
    if (m_useRemote)
        return true;
    return QFileInfo::exists(tileFilePath(z, x, y));
}

QPointF MapTiles::latLonToWorld(double lat, double lon, int z) const
{
    lat = clampLat(lat);
    lon = wrapLon(lon);
    const qreal n = worldSize(z);
    const qreal x = (lon + 180.0) / 360.0 * n;
    const qreal latRad = qDegreesToRadians(lat);
    const qreal y = (1.0 - qLn(qTan(latRad) + 1.0 / qCos(latRad)) / M_PI) / 2.0 * n;
    return {x, y};
}

QPointF MapTiles::worldToLatLon(double wx, double wy, int z) const
{
    const qreal n = worldSize(z);
    wx = std::fmod(wx, n);
    if (wx < 0)
        wx += n;
    wy = qBound(0.0, wy, n);
    const qreal lon = wx / n * 360.0 - 180.0;
    const qreal latRad = std::atan(std::sinh(M_PI * (1.0 - 2.0 * wy / n)));
    return {qRadiansToDegrees(latRad), lon};
}

qreal MapTiles::worldSize(int z) const
{
    return qreal(kTileSize) * qreal(1 << qBound(0, z, 22));
}

int MapTiles::tileCount(int z) const
{
    return 1 << qBound(0, z, 22);
}

QString MapTiles::detectTileRoot() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("maps/tiles")),
        QDir(appDir).filePath(QStringLiteral("../share/mp157-ivi/maps/tiles")),
        QStringLiteral("/opt/ivi/maps/tiles"),
    };
    for (const QString &candidate : candidates) {
        const QString root = QDir(candidate).absolutePath();
        if (QDir(root).exists())
            return root;
    }
    return QDir(appDir).filePath(QStringLiteral("maps/tiles"));
}

QString MapTiles::tileFilePath(int z, int x, int y) const
{
    return QDir(m_tileRoot).filePath(QStringLiteral("%1/%2/%3.png").arg(z).arg(x).arg(y));
}

QString MapTiles::formatTemplate(const QString &tmpl, int z, int x, int y) const
{
    QString out = tmpl;
    out.replace(QStringLiteral("{z}"), QString::number(z));
    out.replace(QStringLiteral("{x}"), QString::number(x));
    out.replace(QStringLiteral("{y}"), QString::number(y));
    return out;
}

void MapTiles::rebuildMarkers()
{
    QVariantList list;

    QVariantMap car;
    car.insert(QStringLiteral("id"), QStringLiteral("vehicle"));
    car.insert(QStringLiteral("name"), QStringLiteral("本车"));
    car.insert(QStringLiteral("lat"), m_vehicleLat);
    car.insert(QStringLiteral("lon"), m_vehicleLon);
    car.insert(QStringLiteral("color"), QStringLiteral("#007AFF"));
    list.append(car);

    QVariantMap home;
    home.insert(QStringLiteral("id"), QStringLiteral("home"));
    home.insert(QStringLiteral("name"), QStringLiteral("家"));
    home.insert(QStringLiteral("lat"), 30.2580);
    home.insert(QStringLiteral("lon"), 120.1350);
    home.insert(QStringLiteral("color"), QStringLiteral("#34C759"));
    list.append(home);

    QVariantMap office;
    office.insert(QStringLiteral("id"), QStringLiteral("office"));
    office.insert(QStringLiteral("name"), QStringLiteral("公司"));
    office.insert(QStringLiteral("lat"), 30.2700);
    office.insert(QStringLiteral("lon"), 120.1500);
    office.insert(QStringLiteral("color"), QStringLiteral("#FF9500"));
    list.append(office);

    if (m_hasSearch) {
        QVariantMap pin;
        pin.insert(QStringLiteral("id"), QStringLiteral("search"));
        pin.insert(QStringLiteral("name"), m_searchName);
        pin.insert(QStringLiteral("lat"), m_searchLat);
        pin.insert(QStringLiteral("lon"), m_searchLon);
        pin.insert(QStringLiteral("color"), QStringLiteral("#AF52DE"));
        list.append(pin);
    }

    m_markers = list;
    emit markersChanged();
}

double MapTiles::wrapLon(double lon)
{
    lon = std::fmod(lon + 180.0, 360.0);
    if (lon < 0)
        lon += 360.0;
    return lon - 180.0;
}

double MapTiles::clampLat(double lat)
{
    return qBound(-85.05112878, lat, 85.05112878);
}

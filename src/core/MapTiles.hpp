#pragma once

#include <QObject>
#include <QPointF>
#include <QString>
#include <QVariantList>

class QNetworkAccessManager;

class MapTiles : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString tileRoot READ tileRoot NOTIFY tileRootChanged)
    Q_PROPERTY(QString tileSource READ tileSource NOTIFY tileSourceChanged)
    Q_PROPERTY(bool useRemote READ useRemote NOTIFY tileSourceChanged)
    Q_PROPERTY(int tileSize READ tileSize CONSTANT)
    Q_PROPERTY(int zoom READ zoom WRITE setZoom NOTIFY viewChanged)
    Q_PROPERTY(int minZoom READ minZoom CONSTANT)
    Q_PROPERTY(int maxZoom READ maxZoom CONSTANT)
    Q_PROPERTY(double centerLat READ centerLat NOTIFY viewChanged)
    Q_PROPERTY(double centerLon READ centerLon NOTIFY viewChanged)
    Q_PROPERTY(double vehicleLat READ vehicleLat NOTIFY vehicleChanged)
    Q_PROPERTY(double vehicleLon READ vehicleLon NOTIFY vehicleChanged)
    Q_PROPERTY(QVariantList markers READ markers NOTIFY markersChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
    Q_PROPERTY(bool hasTiles READ hasTiles NOTIFY tileSourceChanged)
    Q_PROPERTY(QString destinationName READ destinationName NOTIFY destinationChanged)
    Q_PROPERTY(double destinationLat READ destinationLat NOTIFY destinationChanged)
    Q_PROPERTY(double destinationLon READ destinationLon NOTIFY destinationChanged)
    Q_PROPERTY(bool hasDestination READ hasDestination NOTIFY destinationChanged)
public:
    explicit MapTiles(QObject *parent = nullptr);

    QString tileRoot() const;
    QString tileSource() const;
    bool useRemote() const;
    int tileSize() const;
    int zoom() const;
    int minZoom() const;
    int maxZoom() const;
    double centerLat() const;
    double centerLon() const;
    double vehicleLat() const;
    double vehicleLon() const;
    QVariantList markers() const;
    QVariantList searchResults() const;
    bool searching() const;
    bool hasTiles() const;
    QString destinationName() const;
    double destinationLat() const;
    double destinationLon() const;
    bool hasDestination() const;

    void setZoom(int z);

    Q_INVOKABLE void setCenter(double lat, double lon);
    Q_INVOKABLE void panByPixels(double dx, double dy);
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void centerOnVehicle();
    Q_INVOKABLE void setVehiclePosition(double lat, double lon);
    Q_INVOKABLE void searchPlaces(const QString &query);
    Q_INVOKABLE void clearSearch();
    Q_INVOKABLE void goToPlace(const QString &name, double lat, double lon);
    Q_INVOKABLE void stepTowardDestination(qreal factor = 0.08);
    Q_INVOKABLE void refreshTileSource();
    Q_INVOKABLE QString tileUrl(int z, int x, int y) const;
    Q_INVOKABLE bool tileExists(int z, int x, int y) const;
    Q_INVOKABLE QPointF latLonToWorld(double lat, double lon, int z) const;
    Q_INVOKABLE QPointF worldToLatLon(double wx, double wy, int z) const;
    Q_INVOKABLE qreal worldSize(int z) const;
    Q_INVOKABLE int tileCount(int z) const;

signals:
    void tileRootChanged();
    void tileSourceChanged();
    void viewChanged();
    void vehicleChanged();
    void markersChanged();
    void searchResultsChanged();
    void searchingChanged();
    void destinationChanged();

private:
    QString detectTileRoot() const;
    QString tileFilePath(int z, int x, int y) const;
    QString formatTemplate(const QString &tmpl, int z, int x, int y) const;
    void rebuildMarkers();
    void setSearching(bool on);
    void probeRemote();
    static double wrapLon(double lon);
    static double clampLat(double lat);

    QNetworkAccessManager *m_net = nullptr;
    QString m_tileRoot;
    QString m_remoteTemplate;
    bool m_useRemote = false;
    bool m_hasLocal = false;
    int m_zoom = 14;
    double m_centerLat = 30.2630;
    double m_centerLon = 120.1425;
    double m_vehicleLat = 30.2630;
    double m_vehicleLon = 120.1425;
    QString m_searchName;
    double m_searchLat = 0;
    double m_searchLon = 0;
    bool m_hasSearch = false;
    QVariantList m_markers;
    QVariantList m_searchResults;
    bool m_searching = false;
};

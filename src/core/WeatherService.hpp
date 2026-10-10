#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVector>

class GpsSource;
class QNetworkAccessManager;

class WeatherService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString place READ place NOTIFY updated)
    Q_PROPERTY(int temperature READ temperature NOTIFY updated)
    Q_PROPERTY(QString condition READ condition NOTIFY updated)
    Q_PROPERTY(QString kind READ kind NOTIFY updated)
    Q_PROPERTY(bool day READ day NOTIFY updated)
    Q_PROPERTY(QString preview READ preview NOTIFY updated)
    Q_PROPERTY(QString travelAlert READ travelAlert NOTIFY updated)
    Q_PROPERTY(int windKmh READ windKmh NOTIFY updated)
    Q_PROPERTY(int visibilityM READ visibilityM NOTIFY updated)
    Q_PROPERTY(int humidity READ humidity NOTIFY updated)
    Q_PROPERTY(int feelsLike READ feelsLike NOTIFY updated)
    Q_PROPERTY(double uvIndex READ uvIndex NOTIFY updated)
    Q_PROPERTY(bool located READ located NOTIFY updated)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
    Q_PROPERTY(QVariantList cities READ cities NOTIFY citiesChanged)
    Q_PROPERTY(int cityIndex READ cityIndex NOTIFY citiesChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)
    Q_PROPERTY(QVariantList hourlyForecast READ hourlyForecast NOTIFY updated)
    Q_PROPERTY(QVariantList dailyForecast READ dailyForecast NOTIFY updated)
    Q_PROPERTY(QString statusText READ statusText NOTIFY updated)
    Q_PROPERTY(bool fromCache READ fromCache NOTIFY updated)
public:
    explicit WeatherService(QObject *parent = nullptr);
    void setGpsSource(GpsSource *gps);

    QString place() const;
    int temperature() const;
    QString condition() const;
    QString kind() const;
    bool day() const;
    QString preview() const;
    QString travelAlert() const;
    int windKmh() const;
    int visibilityM() const;
    int humidity() const;
    int feelsLike() const;
    double uvIndex() const;
    bool located() const;
    bool refreshing() const;
    QVariantList cities() const;
    int cityIndex() const;
    QVariantList searchResults() const;
    QVariantList hourlyForecast() const;
    QVariantList dailyForecast() const;
    QString statusText() const;
    bool fromCache() const;

    Q_INVOKABLE void setPreview(const QString &mode);
    Q_INVOKABLE void selectCity(int index);
    Q_INVOKABLE void addCity(const QString &name, double lat, double lon);
    Q_INVOKABLE void removeCity(int index);
    Q_INVOKABLE void searchCities(const QString &query);
    Q_INVOKABLE void clearSearch();
    Q_INVOKABLE void refresh();

signals:
    void updated();
    void citiesChanged();
    void searchResultsChanged();
    void refreshingChanged();

private:
    struct City {
        QString name;
        double lat = 0;
        double lon = 0;
        bool autoLocate = false;
    };

    struct LiveHour {
        int code = 0;
        int temp = 0;
        int day = 1;
        double visibility = 10000.0;
        double wind = 0.0;
        double gust = 0.0;
        double precip = 0.0;
        double soil = 0.0;
        int humidity = 0;
        int feels = 0;
        double uv = 0.0;
    };

    void loadCities();
    void saveCities() const;
    void refreshCurrent();
    void setRefreshing(bool on);
    void fetchLocation();
    void fetchLocationFallback();
    void fetchPlace();
    void fetchWeather();
    void fetchWarnings();
    void saveCache() const;
    bool loadCache();
    void applyLive(const LiveHour &now, double precipPrev2h);
    void updateFromHourly();
    void rebuildForecasts(int currentIndex);
    bool currentIsAuto() const;
    void onGpsUpdated();
    bool applyGpsFix();
    static void decodeCode(int code, bool day, QString *kind, QString *condition);
    static void resolveDrivingKind(const LiveHour &now,
                                   double precipPrev2h,
                                   const QString &warning,
                                   QString *kind,
                                   QString *condition);
    static bool isRainKind(const QString &kind);
    static bool isSnowKind(const QString &kind);
    static bool warningIsTyphoon(const QString &warning);
    static QString weekdayLabel(const QDate &date);
    static LiveHour sampleHourly(const QVector<QDateTime> &times,
                                 const QVector<int> &codes,
                                 const QVector<int> &temps,
                                 const QVector<int> &days,
                                 const QVector<double> &vis,
                                 const QVector<double> &wind,
                                 const QVector<double> &gust,
                                 const QVector<double> &precip,
                                 const QVector<double> &soil,
                                 const QVector<int> &humidity,
                                 const QVector<int> &feels,
                                 const QVector<double> &uv,
                                 int index,
                                 double frac);

    QNetworkAccessManager *m_net = nullptr;
    GpsSource *m_gps = nullptr;
    QTimer m_timer;
    QTimer m_liveTimer;
    double m_weatherLat = 0;
    double m_weatherLon = 0;
    QVector<QDateTime> m_hourlyTime;
    QVector<int> m_hourlyCodes;
    QVector<int> m_hourlyTemps;
    QVector<int> m_hourlyDay;
    QVector<double> m_hourlyVis;
    QVector<double> m_hourlyWind;
    QVector<double> m_hourlyGust;
    QVector<double> m_hourlyPrecip;
    QVector<double> m_hourlySoil;
    QVector<int> m_hourlyHumidity;
    QVector<int> m_hourlyFeels;
    QVector<double> m_hourlyUv;
    QVariantList m_hourlyForecast;
    QVariantList m_dailyForecast;
    QVector<City> m_cities;
    QVariantList m_searchResults;
    int m_cityIndex = 0;
    double m_lat = 0;
    double m_lon = 0;
    QString m_place;
    int m_temperature = 0;
    QString m_condition;
    QString m_kind;
    bool m_day = true;
    QString m_preview;
    QString m_travelAlert;
    int m_windKmh = 0;
    int m_visibilityM = 10000;
    int m_humidity = 0;
    int m_feelsLike = 0;
    double m_uvIndex = 0;
    bool m_located = false;
    bool m_refreshing = false;
    bool m_fromCache = false;
    QString m_statusText;
};

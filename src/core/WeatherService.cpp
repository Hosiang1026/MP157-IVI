#include "WeatherService.hpp"

#include "GpsSource.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDate>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QUrlQuery>
#include <QtMath>

void WeatherService::decodeCode(int code, bool day, QString *kind, QString *condition)
{
    *kind = QStringLiteral("cloudy");
    *condition = QStringLiteral("多云");
    if (code == 0 || code == 1) {
        *kind = QStringLiteral("clear");
        *condition = day ? QStringLiteral("晴") : QStringLiteral("晴夜");
    } else if (code == 2) {
        *kind = QStringLiteral("cloudy");
        *condition = QStringLiteral("多云");
    } else if (code == 3) {
        *kind = QStringLiteral("overcast");
        *condition = QStringLiteral("阴");
    } else if (code == 5) {
        *kind = QStringLiteral("haze");
        *condition = QStringLiteral("霾");
    } else if (code == 36) {
        *kind = QStringLiteral("sandLift");
        *condition = QStringLiteral("扬沙");
    } else if (code >= 30 && code <= 35) {
        *kind = QStringLiteral("dust");
        *condition = QStringLiteral("沙尘");
    } else if (code == 45 || code == 48) {
        *kind = QStringLiteral("fog");
        *condition = QStringLiteral("雾");
    } else if (code == 56 || code == 57 || code == 66 || code == 67) {
        *kind = QStringLiteral("freezeRain");
        *condition = QStringLiteral("冻雨");
    } else if (code == 68 || code == 69 || code == 70) {
        *kind = QStringLiteral("sleet");
        *condition = QStringLiteral("雨夹雪");
    } else if (code >= 51 && code <= 55) {
        *kind = QStringLiteral("rain");
        *condition = QStringLiteral("小雨");
    } else if (code == 61 || code == 63 || code == 80 || code == 81) {
        *kind = QStringLiteral("rainMid");
        *condition = QStringLiteral("中雨");
    } else if (code == 65 || code == 82) {
        *kind = QStringLiteral("rainHard");
        *condition = QStringLiteral("暴雨");
    } else if (code == 71 || code == 77 || code == 85) {
        *kind = QStringLiteral("snow");
        *condition = QStringLiteral("小雪");
    } else if (code == 73) {
        *kind = QStringLiteral("snowMid");
        *condition = QStringLiteral("中雪");
    } else if (code == 75 || code == 86) {
        *kind = QStringLiteral("snowHard");
        *condition = QStringLiteral("大雪");
    } else if (code == 96 || code == 99) {
        *kind = QStringLiteral("hail");
        *condition = QStringLiteral("冰雹");
    } else if (code >= 95) {
        *kind = QStringLiteral("thunder");
        *condition = QStringLiteral("雷雨");
    }
}

bool WeatherService::isRainKind(const QString &kind)
{
    return kind == QLatin1String("rain") || kind == QLatin1String("rainMid") || kind == QLatin1String("rainHard")
        || kind == QLatin1String("thunder") || kind == QLatin1String("sleet") || kind == QLatin1String("freezeRain")
        || kind == QLatin1String("ponding") || kind == QLatin1String("typhoon");
}

bool WeatherService::isSnowKind(const QString &kind)
{
    return kind == QLatin1String("snow") || kind == QLatin1String("snowMid") || kind == QLatin1String("snowHard")
        || kind == QLatin1String("sleet") || kind == QLatin1String("hail") || kind == QLatin1String("blizzard");
}

bool WeatherService::warningIsTyphoon(const QString &warning)
{
    if (warning.isEmpty())
        return false;
    const QString w = warning.toLower();
    static const QStringList keys = {
        QStringLiteral("typhoon"),
        QStringLiteral("tropical cyclone"),
        QStringLiteral("tropical storm"),
        QStringLiteral("hurricane"),
        QStringLiteral("台风"),
        QStringLiteral("热带风暴"),
        QStringLiteral("热带气旋"),
        QStringLiteral("飓风"),
    };
    for (const QString &key : keys) {
        if (w.contains(key))
            return true;
    }
    return false;
}

void WeatherService::resolveDrivingKind(const LiveHour &now,
                                        double precipPrev2h,
                                        const QString &warning,
                                        QString *kind,
                                        QString *condition)
{
    decodeCode(now.code, now.day != 0, kind, condition);

    const double visKm = now.visibility / 1000.0;
    const double gust = now.gust > 0.0 ? now.gust : now.wind;
    const double soil = now.soil != 0.0 ? now.soil : static_cast<double>(now.temp) - 2.0;

    if (warningIsTyphoon(warning) || (gust >= 75.0 && isRainKind(*kind))) {
        *kind = QStringLiteral("typhoon");
        *condition = QStringLiteral("台风");
        return;
    }
    if ((isSnowKind(*kind) && gust >= 40.0) || (now.temp <= 1 && visKm < 1.2 && isSnowKind(*kind))) {
        *kind = QStringLiteral("blizzard");
        *condition = QStringLiteral("暴雪");
        return;
    }
    if (*kind == QLatin1String("rainHard") || *kind == QLatin1String("thunder")
        || (isRainKind(*kind) && now.precip >= 6.0) || (now.precip >= 3.5 && visKm < 2.5)) {
        *kind = QStringLiteral("ponding");
        *condition = QStringLiteral("积水");
        return;
    }
    if (now.temp <= 2 && soil <= 1.0
        && (*kind == QLatin1String("clear") || *kind == QLatin1String("cloudy")
            || *kind == QLatin1String("overcast"))) {
        *kind = QStringLiteral("frost");
        *condition = QStringLiteral("霜");
        return;
    }
    if (precipPrev2h >= 0.8 && now.precip < 0.12
        && (*kind == QLatin1String("clear") || *kind == QLatin1String("cloudy")
            || *kind == QLatin1String("overcast"))
        && now.temp > 0 && now.temp < 36) {
        *kind = QStringLiteral("wetRoad");
        *condition = QStringLiteral("湿滑");
        return;
    }
    if (gust >= 50.0 || now.wind >= 42.0) {
        *kind = QStringLiteral("wind");
        *condition = QStringLiteral("大风");
        return;
    }
}

WeatherService::LiveHour WeatherService::sampleHourly(const QVector<QDateTime> &times,
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
                                                      double frac)
{
    LiveHour h;
    const int i1 = qBound(0, index, codes.size() - 1);
    const int i2 = qMin(i1 + 1, codes.size() - 1);
    const auto lerp = [frac](double a, double b) { return a + (b - a) * frac; };
    h.code = frac < 0.65 ? codes.at(i1) : codes.at(i2);
    h.temp = qRound(lerp(temps.at(i1), temps.at(i2)));
    h.day = days.at(i1);
    if (!vis.isEmpty())
        h.visibility = lerp(vis.at(i1), vis.at(i2));
    if (!wind.isEmpty())
        h.wind = lerp(wind.at(i1), wind.at(i2));
    if (!gust.isEmpty())
        h.gust = lerp(gust.at(i1), gust.at(i2));
    if (!precip.isEmpty())
        h.precip = lerp(precip.at(i1), precip.at(i2));
    if (!soil.isEmpty())
        h.soil = lerp(soil.at(i1), soil.at(i2));
    if (!humidity.isEmpty())
        h.humidity = qRound(lerp(humidity.at(i1), humidity.at(i2)));
    if (!feels.isEmpty())
        h.feels = qRound(lerp(feels.at(i1), feels.at(i2)));
    if (!uv.isEmpty())
        h.uv = lerp(uv.at(i1), uv.at(i2));
    Q_UNUSED(times);
    return h;
}

QString WeatherService::weekdayLabel(const QDate &date)
{
    const QDate today = QDate::currentDate();
    if (date == today)
        return QStringLiteral("今天");
    if (date == today.addDays(1))
        return QStringLiteral("明天");
    static const QStringList names = {
        QStringLiteral("周一"),
        QStringLiteral("周二"),
        QStringLiteral("周三"),
        QStringLiteral("周四"),
        QStringLiteral("周五"),
        QStringLiteral("周六"),
        QStringLiteral("周日"),
    };
    return names.at(date.dayOfWeek() - 1);
}

WeatherService::WeatherService(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
    loadCities();
    loadCache();
    m_timer.setInterval(5 * 60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, &WeatherService::refreshCurrent);
    m_timer.start();
    m_liveTimer.setInterval(60 * 1000);
    connect(&m_liveTimer, &QTimer::timeout, this, &WeatherService::updateFromHourly);
    m_liveTimer.start();
    QTimer::singleShot(800, this, &WeatherService::refreshCurrent);
}

void WeatherService::setGpsSource(GpsSource *gps)
{
    if (m_gps == gps)
        return;
    if (m_gps)
        disconnect(m_gps, nullptr, this, nullptr);
    m_gps = gps;
    if (!m_gps)
        return;
    connect(m_gps, &GpsSource::positionUpdated, this, &WeatherService::onGpsUpdated);
    if (m_gps->hasFix())
        QTimer::singleShot(0, this, &WeatherService::onGpsUpdated);
}

bool WeatherService::applyGpsFix()
{
    if (!m_gps || !m_gps->hasFix())
        return false;
    m_lat = m_gps->latitude();
    m_lon = m_gps->longitude();
    m_located = true;
    return true;
}

void WeatherService::onGpsUpdated()
{
    if (!applyGpsFix())
        return;
    const double dLat = qAbs(m_lat - m_weatherLat);
    const double dLon = qAbs(m_lon - m_weatherLon);
    const bool first = m_weatherLat == 0.0 && m_weatherLon == 0.0;
    if (!first && dLat < 0.03 && dLon < 0.03)
        return;
    if (m_refreshing)
        return;
    setRefreshing(true);
    m_statusText = m_gps->demoMode() ? QStringLiteral("演示定位") : QStringLiteral("GPS 定位");
    emit updated();
    fetchPlace();
}

QString WeatherService::place() const
{
    return m_place;
}

int WeatherService::temperature() const
{
    return m_temperature;
}

QString WeatherService::condition() const
{
    if (m_preview == QLatin1String("clear"))
        return QStringLiteral("晴");
    if (m_preview == QLatin1String("night"))
        return QStringLiteral("晴夜");
    if (m_preview == QLatin1String("cloudy"))
        return QStringLiteral("多云");
    if (m_preview == QLatin1String("overcast"))
        return QStringLiteral("阴");
    if (m_preview == QLatin1String("fog"))
        return QStringLiteral("雾");
    if (m_preview == QLatin1String("haze"))
        return QStringLiteral("霾");
    if (m_preview == QLatin1String("dust"))
        return QStringLiteral("沙尘");
    if (m_preview == QLatin1String("sandLift"))
        return QStringLiteral("扬沙");
    if (m_preview == QLatin1String("rain"))
        return QStringLiteral("小雨");
    if (m_preview == QLatin1String("rainMid"))
        return QStringLiteral("中雨");
    if (m_preview == QLatin1String("rainHard"))
        return QStringLiteral("暴雨");
    if (m_preview == QLatin1String("ponding"))
        return QStringLiteral("积水");
    if (m_preview == QLatin1String("wetRoad"))
        return QStringLiteral("湿滑");
    if (m_preview == QLatin1String("wind"))
        return QStringLiteral("大风");
    if (m_preview == QLatin1String("blizzard"))
        return QStringLiteral("暴雪");
    if (m_preview == QLatin1String("frost"))
        return QStringLiteral("霜");
    if (m_preview == QLatin1String("typhoon"))
        return QStringLiteral("台风");
    if (m_preview == QLatin1String("thunder"))
        return QStringLiteral("雷雨");
    if (m_preview == QLatin1String("sleet"))
        return QStringLiteral("雨夹雪");
    if (m_preview == QLatin1String("freezeRain"))
        return QStringLiteral("冻雨");
    if (m_preview == QLatin1String("snow"))
        return QStringLiteral("小雪");
    if (m_preview == QLatin1String("snowMid"))
        return QStringLiteral("中雪");
    if (m_preview == QLatin1String("snowHard"))
        return QStringLiteral("大雪");
    if (m_preview == QLatin1String("hail"))
        return QStringLiteral("冰雹");
    return m_condition;
}

QString WeatherService::kind() const
{
    if (m_preview == QLatin1String("night") || m_preview == QLatin1String("clear"))
        return QStringLiteral("clear");
    if (!m_preview.isEmpty())
        return m_preview;
    return m_kind;
}

bool WeatherService::day() const
{
    if (m_preview == QLatin1String("night"))
        return false;
    if (m_preview == QLatin1String("clear"))
        return true;
    return m_day;
}

QString WeatherService::preview() const
{
    return m_preview;
}

QString WeatherService::travelAlert() const
{
    if (!m_preview.isEmpty())
        return QString();
    return m_travelAlert;
}

int WeatherService::windKmh() const
{
    return m_windKmh;
}

int WeatherService::visibilityM() const
{
    return m_visibilityM;
}

bool WeatherService::located() const
{
    return m_located;
}

int WeatherService::humidity() const
{
    return m_humidity;
}

int WeatherService::feelsLike() const
{
    return m_feelsLike;
}

double WeatherService::uvIndex() const
{
    return m_uvIndex;
}

bool WeatherService::refreshing() const
{
    return m_refreshing;
}

QVariantList WeatherService::hourlyForecast() const
{
    return m_hourlyForecast;
}

QVariantList WeatherService::dailyForecast() const
{
    return m_dailyForecast;
}

QString WeatherService::statusText() const
{
    return m_statusText;
}

bool WeatherService::fromCache() const
{
    return m_fromCache;
}

void WeatherService::saveCache() const
{
    QJsonObject root;
    root.insert(QStringLiteral("place"), m_place);
    root.insert(QStringLiteral("lat"), m_lat);
    root.insert(QStringLiteral("lon"), m_lon);
    root.insert(QStringLiteral("temperature"), m_temperature);
    root.insert(QStringLiteral("condition"), m_condition);
    root.insert(QStringLiteral("kind"), m_kind);
    root.insert(QStringLiteral("day"), m_day);
    root.insert(QStringLiteral("windKmh"), m_windKmh);
    root.insert(QStringLiteral("visibilityM"), m_visibilityM);
    root.insert(QStringLiteral("humidity"), m_humidity);
    root.insert(QStringLiteral("feelsLike"), m_feelsLike);
    root.insert(QStringLiteral("uvIndex"), m_uvIndex);
    root.insert(QStringLiteral("travelAlert"), m_travelAlert);
    root.insert(QStringLiteral("hourly"), QJsonArray::fromVariantList(m_hourlyForecast));
    root.insert(QStringLiteral("daily"), QJsonArray::fromVariantList(m_dailyForecast));
    const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("weather-cache.json"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool WeatherService::loadCache()
{
    const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("weather-cache.json"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.isEmpty())
        return false;
    m_place = root.value(QStringLiteral("place")).toString(m_place);
    m_lat = root.value(QStringLiteral("lat")).toDouble(m_lat);
    m_lon = root.value(QStringLiteral("lon")).toDouble(m_lon);
    m_temperature = root.value(QStringLiteral("temperature")).toInt(m_temperature);
    m_condition = root.value(QStringLiteral("condition")).toString(m_condition);
    m_kind = root.value(QStringLiteral("kind")).toString(m_kind);
    m_day = root.value(QStringLiteral("day")).toBool(m_day);
    m_windKmh = root.value(QStringLiteral("windKmh")).toInt(m_windKmh);
    m_visibilityM = root.value(QStringLiteral("visibilityM")).toInt(m_visibilityM);
    m_humidity = root.value(QStringLiteral("humidity")).toInt(m_humidity);
    m_feelsLike = root.value(QStringLiteral("feelsLike")).toInt(m_feelsLike);
    m_uvIndex = root.value(QStringLiteral("uvIndex")).toDouble(m_uvIndex);
    m_travelAlert = root.value(QStringLiteral("travelAlert")).toString();
    m_hourlyForecast = root.value(QStringLiteral("hourly")).toArray().toVariantList();
    m_dailyForecast = root.value(QStringLiteral("daily")).toArray().toVariantList();
    m_fromCache = true;
    m_located = true;
    if (m_statusText.isEmpty())
        m_statusText = QStringLiteral("离线缓存");
    return !m_condition.isEmpty() || !m_hourlyForecast.isEmpty();
}

QVariantList WeatherService::cities() const
{
    QVariantList list;
    for (const City &city : m_cities) {
        QVariantMap item;
        item.insert(QStringLiteral("name"), city.name);
        item.insert(QStringLiteral("lat"), city.lat);
        item.insert(QStringLiteral("lon"), city.lon);
        item.insert(QStringLiteral("autoLocate"), city.autoLocate);
        list.append(item);
    }
    return list;
}

int WeatherService::cityIndex() const
{
    return m_cityIndex;
}

QVariantList WeatherService::searchResults() const
{
    return m_searchResults;
}

void WeatherService::setPreview(const QString &mode)
{
    if (m_preview == mode)
        return;
    m_preview = mode;
    emit updated();
}

void WeatherService::loadCities()
{
    m_cities.clear();
    City autoCity;
    autoCity.name = QStringLiteral("当前位置");
    autoCity.autoLocate = true;
    m_cities.append(autoCity);
    m_cityIndex = 0;
    QSettings settings;
    settings.remove(QStringLiteral("weather/cities"));
    settings.remove(QStringLiteral("weather/cityIndex"));
}

void WeatherService::saveCities() const
{
    QVariantList list;
    for (const City &city : m_cities) {
        QVariantMap item;
        item.insert(QStringLiteral("name"), city.name);
        item.insert(QStringLiteral("lat"), city.lat);
        item.insert(QStringLiteral("lon"), city.lon);
        item.insert(QStringLiteral("autoLocate"), city.autoLocate);
        list.append(item);
    }
    QSettings settings;
    settings.setValue(QStringLiteral("weather/cities"), list);
    settings.setValue(QStringLiteral("weather/cityIndex"), m_cityIndex);
}

bool WeatherService::currentIsAuto() const
{
    return m_cityIndex >= 0 && m_cityIndex < m_cities.size() && m_cities.at(m_cityIndex).autoLocate;
}

void WeatherService::setRefreshing(bool on)
{
    if (m_refreshing == on)
        return;
    m_refreshing = on;
    emit refreshingChanged();
}

void WeatherService::refresh()
{
    setRefreshing(true);
    refreshCurrent();
}

void WeatherService::refreshCurrent()
{
    fetchLocation();
}

void WeatherService::selectCity(int)
{
}

void WeatherService::addCity(const QString &, double, double)
{
    clearSearch();
}

void WeatherService::removeCity(int)
{
}

void WeatherService::searchCities(const QString &query)
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        clearSearch();
        return;
    }
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
        emit searchResultsChanged();
    });
}

void WeatherService::clearSearch()
{
    if (m_searchResults.isEmpty())
        return;
    m_searchResults.clear();
    emit searchResultsChanged();
}

void WeatherService::fetchLocation()
{
    if (applyGpsFix()) {
        m_statusText = m_gps->demoMode() ? QStringLiteral("演示定位") : QStringLiteral("GPS 定位");
        emit updated();
        fetchPlace();
        return;
    }
    m_statusText = QStringLiteral("网络定位中…");
    emit updated();
    QNetworkRequest req(QUrl(QStringLiteral("http://ip-api.com/json/?lang=zh-CN&fields=status,city,lat,lon")));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mp157-ivi"));
    req.setTransferTimeout(8000);
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() != QNetworkReply::NoError || obj.value(QStringLiteral("status")).toString() != QLatin1String("success")) {
            fetchLocationFallback();
            return;
        }
        m_lat = obj.value(QStringLiteral("lat")).toDouble();
        m_lon = obj.value(QStringLiteral("lon")).toDouble();
        if (!m_located) {
            m_located = true;
            emit updated();
        }
        fetchPlace();
    });
}

void WeatherService::fetchLocationFallback()
{
    QNetworkRequest req(QUrl(QStringLiteral("https://ipwho.is/")));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mp157-ivi"));
    req.setTransferTimeout(8000);
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() != QNetworkReply::NoError || !obj.value(QStringLiteral("success")).toBool()) {
            if (loadCache()) {
                m_statusText = QStringLiteral("离线缓存");
                setRefreshing(false);
                emit updated();
                return;
            }
            m_lat = 30.2741;
            m_lon = 120.1551;
            m_place = QStringLiteral("杭州");
            m_located = true;
            m_statusText = QStringLiteral("定位失败，已用默认城市");
            emit updated();
            fetchWeather();
            return;
        }
        m_lat = obj.value(QStringLiteral("latitude")).toDouble();
        m_lon = obj.value(QStringLiteral("longitude")).toDouble();
        if (!m_located) {
            m_located = true;
            emit updated();
        }
        fetchPlace();
    });
}

void WeatherService::fetchPlace()
{
    QUrl url(QStringLiteral("https://api.bigdatacloud.net/data/reverse-geocode-client"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("latitude"), QString::number(m_lat, 'f', 4));
    query.addQueryItem(QStringLiteral("longitude"), QString::number(m_lon, 'f', 4));
    query.addQueryItem(QStringLiteral("localityLanguage"), QStringLiteral("zh"));
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mp157-ivi"));
    req.setTransferTimeout(8000);
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        QString name;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
            const QStringList keys = {QStringLiteral("city"), QStringLiteral("locality"), QStringLiteral("principalSubdivision")};
            for (const QString &key : keys) {
                const QString value = obj.value(key).toString().trimmed();
                bool han = false;
                for (const QChar &ch : value) {
                    if (ch.unicode() >= 0x4E00 && ch.unicode() <= 0x9FFF) {
                        han = true;
                        break;
                    }
                }
                if (han) {
                    name = value;
                    break;
                }
            }
            if (name.endsWith(QStringLiteral("市")))
                name.chop(1);
        }
        if (!name.isEmpty()) {
            m_place = name;
            if (!m_cities.isEmpty() && m_cities.first().name != name) {
                m_cities.first().name = name;
                emit citiesChanged();
            }
        }
        m_weatherLat = m_lat;
        m_weatherLon = m_lon;
        fetchWeather();
        QTimer::singleShot(2000, this, &WeatherService::fetchWarnings);
    });
}

void WeatherService::fetchWarnings()
{
    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/warnings"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("latitude"), QString::number(m_lat, 'f', 4));
    query.addQueryItem(QStringLiteral("longitude"), QString::number(m_lon, 'f', 4));
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mp157-ivi"));
    req.setTransferTimeout(8000);
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        QString alert;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
            if (root.value(QStringLiteral("error")).toBool())
                return;
            const QJsonArray warnings = root.value(QStringLiteral("warnings")).toArray();
            for (const QJsonValue &item : warnings) {
                const QJsonObject w = item.toObject();
                const QString headline = w.value(QStringLiteral("headline")).toString().trimmed();
                const QString event = w.value(QStringLiteral("event")).toString().trimmed();
                const QString text = headline.isEmpty() ? event : headline;
                if (!text.isEmpty()) {
                    alert = text.length() > 42 ? text.left(40) + QStringLiteral("…") : text;
                    break;
                }
            }
        }
        if (m_travelAlert != alert) {
            m_travelAlert = alert;
            if (m_preview.isEmpty() && !m_hourlyTime.isEmpty())
                updateFromHourly();
            else
                emit updated();
        }
    });
}

void WeatherService::fetchWeather()
{
    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("latitude"), QString::number(m_lat, 'f', 4));
    query.addQueryItem(QStringLiteral("longitude"), QString::number(m_lon, 'f', 4));
    query.addQueryItem(QStringLiteral("current"),
                       QStringLiteral("temperature_2m,weather_code,is_day,visibility,wind_speed_10m,wind_gusts_10m,relative_humidity_2m,apparent_temperature"));
    query.addQueryItem(QStringLiteral("hourly"),
                       QStringLiteral("temperature_2m,weather_code,is_day,visibility,wind_speed_10m,wind_gusts_10m,precipitation,soil_temperature_0cm,relative_humidity_2m,apparent_temperature,uv_index"));
    query.addQueryItem(QStringLiteral("daily"),
                       QStringLiteral("weather_code,temperature_2m_max,temperature_2m_min,uv_index_max,precipitation_sum"));
    query.addQueryItem(QStringLiteral("forecast_hours"), QStringLiteral("48"));
    query.addQueryItem(QStringLiteral("forecast_days"), QStringLiteral("7"));
    query.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mp157-ivi"));
    req.setTransferTimeout(8000);
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (loadCache()) {
                m_statusText = QStringLiteral("离线缓存");
                setRefreshing(false);
                emit updated();
                return;
            }
            m_statusText = QStringLiteral("网络不可用");
            setRefreshing(false);
            emit updated();
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonObject hourly = root.value(QStringLiteral("hourly")).toObject();
        const QJsonArray times = hourly.value(QStringLiteral("time")).toArray();
        const QJsonArray codes = hourly.value(QStringLiteral("weather_code")).toArray();
        const QJsonArray temps = hourly.value(QStringLiteral("temperature_2m")).toArray();
        const QJsonArray days = hourly.value(QStringLiteral("is_day")).toArray();
        const QJsonArray visArr = hourly.value(QStringLiteral("visibility")).toArray();
        const QJsonArray windArr = hourly.value(QStringLiteral("wind_speed_10m")).toArray();
        const QJsonArray gustArr = hourly.value(QStringLiteral("wind_gusts_10m")).toArray();
        const QJsonArray precipArr = hourly.value(QStringLiteral("precipitation")).toArray();
        const QJsonArray soilArr = hourly.value(QStringLiteral("soil_temperature_0cm")).toArray();
        const QJsonArray humidityArr = hourly.value(QStringLiteral("relative_humidity_2m")).toArray();
        const QJsonArray feelsArr = hourly.value(QStringLiteral("apparent_temperature")).toArray();
        const QJsonArray uvArr = hourly.value(QStringLiteral("uv_index")).toArray();

        m_hourlyTime.clear();
        m_hourlyCodes.clear();
        m_hourlyTemps.clear();
        m_hourlyDay.clear();
        m_hourlyVis.clear();
        m_hourlyWind.clear();
        m_hourlyGust.clear();
        m_hourlyPrecip.clear();
        m_hourlySoil.clear();
        m_hourlyHumidity.clear();
        m_hourlyFeels.clear();
        m_hourlyUv.clear();

        const int n = qMin(times.size(), qMin(codes.size(), qMin(temps.size(), days.size())));
        m_hourlyTime.reserve(n);
        m_hourlyCodes.reserve(n);
        m_hourlyTemps.reserve(n);
        m_hourlyDay.reserve(n);
        m_hourlyVis.reserve(n);
        m_hourlyWind.reserve(n);
        m_hourlyGust.reserve(n);
        m_hourlyPrecip.reserve(n);
        m_hourlySoil.reserve(n);
        m_hourlyHumidity.reserve(n);
        m_hourlyFeels.reserve(n);
        m_hourlyUv.reserve(n);

        for (int i = 0; i < n; ++i) {
            const QDateTime when = QDateTime::fromString(times.at(i).toString(), Qt::ISODate);
            if (!when.isValid())
                continue;
            m_hourlyTime.append(when);
            m_hourlyCodes.append(codes.at(i).toInt());
            m_hourlyTemps.append(qRound(temps.at(i).toDouble()));
            m_hourlyDay.append(days.at(i).toInt());
            m_hourlyVis.append(i < visArr.size() ? visArr.at(i).toDouble(10000.0) : 10000.0);
            m_hourlyWind.append(i < windArr.size() ? windArr.at(i).toDouble() : 0.0);
            m_hourlyGust.append(i < gustArr.size() ? gustArr.at(i).toDouble() : 0.0);
            m_hourlyPrecip.append(i < precipArr.size() ? precipArr.at(i).toDouble() : 0.0);
            m_hourlySoil.append(i < soilArr.size() ? soilArr.at(i).toDouble() : 0.0);
            m_hourlyHumidity.append(i < humidityArr.size() ? qRound(humidityArr.at(i).toDouble()) : 0);
            m_hourlyFeels.append(i < feelsArr.size() ? qRound(feelsArr.at(i).toDouble()) : m_hourlyTemps.last());
            m_hourlyUv.append(i < uvArr.size() ? uvArr.at(i).toDouble() : 0.0);
        }

        m_dailyForecast.clear();
        const QJsonObject daily = root.value(QStringLiteral("daily")).toObject();
        const QJsonArray dTimes = daily.value(QStringLiteral("time")).toArray();
        const QJsonArray dCodes = daily.value(QStringLiteral("weather_code")).toArray();
        const QJsonArray dMax = daily.value(QStringLiteral("temperature_2m_max")).toArray();
        const QJsonArray dMin = daily.value(QStringLiteral("temperature_2m_min")).toArray();
        const QJsonArray dUv = daily.value(QStringLiteral("uv_index_max")).toArray();
        const int dn = qMin(dTimes.size(), qMin(dCodes.size(), qMin(dMax.size(), dMin.size())));
        for (int i = 0; i < dn; ++i) {
            const QDate date = QDate::fromString(dTimes.at(i).toString(), Qt::ISODate);
            if (!date.isValid())
                continue;
            QString kind;
            QString condition;
            decodeCode(dCodes.at(i).toInt(), true, &kind, &condition);
            QVariantMap item;
            item.insert(QStringLiteral("date"), date.toString(QStringLiteral("M/d")));
            item.insert(QStringLiteral("weekday"), weekdayLabel(date));
            item.insert(QStringLiteral("tempMax"), qRound(dMax.at(i).toDouble()));
            item.insert(QStringLiteral("tempMin"), qRound(dMin.at(i).toDouble()));
            item.insert(QStringLiteral("kind"), kind);
            item.insert(QStringLiteral("condition"), condition);
            item.insert(QStringLiteral("uv"), i < dUv.size() ? dUv.at(i).toDouble() : 0.0);
            m_dailyForecast.append(item);
        }

        if (m_preview.isEmpty())
            updateFromHourly();
        else {
            const QJsonObject current = root.value(QStringLiteral("current")).toObject();
            if (!current.isEmpty()) {
                LiveHour now;
                now.code = current.value(QStringLiteral("weather_code")).toInt();
                now.temp = qRound(current.value(QStringLiteral("temperature_2m")).toDouble());
                now.day = current.value(QStringLiteral("is_day")).toInt();
                now.visibility = current.value(QStringLiteral("visibility")).toDouble(10000.0);
                now.wind = current.value(QStringLiteral("wind_speed_10m")).toDouble();
                now.gust = current.value(QStringLiteral("wind_gusts_10m")).toDouble();
                now.humidity = qRound(current.value(QStringLiteral("relative_humidity_2m")).toDouble());
                now.feels = qRound(current.value(QStringLiteral("apparent_temperature")).toDouble());
                now.precip = 0.0;
                if (!m_hourlyPrecip.isEmpty())
                    now.precip = m_hourlyPrecip.first();
                if (!m_hourlySoil.isEmpty())
                    now.soil = m_hourlySoil.first();
                if (!m_hourlyUv.isEmpty())
                    now.uv = m_hourlyUv.first();
                rebuildForecasts(0);
                applyLive(now, 0.0);
                m_fromCache = false;
                m_statusText.clear();
                saveCache();
            } else {
                emit updated();
            }
        }
        setRefreshing(false);
    });
}

void WeatherService::rebuildForecasts(int currentIndex)
{
    m_hourlyForecast.clear();
    const int start = qBound(0, currentIndex, m_hourlyTime.size());
    const int end = qMin(start + 24, m_hourlyTime.size());
    for (int i = start; i < end; ++i) {
        QString kind;
        QString condition;
        decodeCode(m_hourlyCodes.at(i), m_hourlyDay.at(i) != 0, &kind, &condition);
        QVariantMap item;
        item.insert(QStringLiteral("hour"), m_hourlyTime.at(i).toString(QStringLiteral("HH:mm")));
        item.insert(QStringLiteral("temp"), m_hourlyTemps.at(i));
        item.insert(QStringLiteral("kind"), kind);
        item.insert(QStringLiteral("condition"), condition);
        item.insert(QStringLiteral("day"), m_hourlyDay.at(i) != 0);
        item.insert(QStringLiteral("now"), i == start);
        m_hourlyForecast.append(item);
    }
}

void WeatherService::updateFromHourly()
{
    if (!m_preview.isEmpty() || m_hourlyTime.isEmpty())
        return;
    const QDateTime now = QDateTime::currentDateTime();
    int index = 0;
    while (index + 1 < m_hourlyTime.size() && now >= m_hourlyTime.at(index + 1))
        ++index;
    index = qBound(0, index, m_hourlyTime.size() - 1);
    qreal frac = 0.0;
    if (index + 1 < m_hourlyTime.size()) {
        const qint64 span = m_hourlyTime.at(index).secsTo(m_hourlyTime.at(index + 1));
        if (span > 0)
            frac = qreal(m_hourlyTime.at(index).secsTo(now)) / span;
    }
    frac = qBound(0.0, frac, 1.0);

    double precipPrev2h = 0.0;
    for (int j = qMax(0, index - 2); j < index; ++j) {
        if (j < m_hourlyPrecip.size())
            precipPrev2h += m_hourlyPrecip.at(j);
    }

    const LiveHour sample = sampleHourly(m_hourlyTime,
                                         m_hourlyCodes,
                                         m_hourlyTemps,
                                         m_hourlyDay,
                                         m_hourlyVis,
                                         m_hourlyWind,
                                         m_hourlyGust,
                                         m_hourlyPrecip,
                                         m_hourlySoil,
                                         m_hourlyHumidity,
                                         m_hourlyFeels,
                                         m_hourlyUv,
                                         index,
                                         frac);
    rebuildForecasts(index);
    applyLive(sample, precipPrev2h);
}

void WeatherService::applyLive(const LiveHour &now, double precipPrev2h)
{
    QString kind;
    QString condition;
    resolveDrivingKind(now, precipPrev2h, m_travelAlert, &kind, &condition);

    m_temperature = now.temp;
    m_condition = condition;
    m_kind = kind;
    m_day = now.day != 0;
    m_windKmh = qRound(now.gust > 0.0 ? now.gust : now.wind);
    m_visibilityM = qRound(now.visibility);
    m_humidity = now.humidity;
    m_feelsLike = now.feels != 0 ? now.feels : now.temp;
    m_uvIndex = now.uv;

    emit updated();
}

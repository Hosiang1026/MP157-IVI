#include "WeatherService.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QTimer>
#include <QUrlQuery>

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
    Q_UNUSED(times);
    return h;
}

WeatherService::WeatherService(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
    m_timer.setInterval(5 * 60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, &WeatherService::fetchLocation);
    m_timer.start();
    m_liveTimer.setInterval(60 * 1000);
    connect(&m_liveTimer, &QTimer::timeout, this, &WeatherService::updateFromHourly);
    m_liveTimer.start();
    QTimer::singleShot(800, this, &WeatherService::fetchLocation);
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

void WeatherService::setPreview(const QString &mode)
{
    if (m_preview == mode)
        return;
    m_preview = mode;
    emit updated();
}

void WeatherService::fetchLocation()
{
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
        if (reply->error() != QNetworkReply::NoError || !obj.value(QStringLiteral("success")).toBool())
            return;
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
        if (!name.isEmpty())
            m_place = name;
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
                       QStringLiteral("temperature_2m,weather_code,is_day,visibility,wind_speed_10m,wind_gusts_10m"));
    query.addQueryItem(QStringLiteral("hourly"),
                       QStringLiteral("temperature_2m,weather_code,is_day,visibility,wind_speed_10m,wind_gusts_10m,precipitation,soil_temperature_0cm"));
    query.addQueryItem(QStringLiteral("forecast_hours"), QStringLiteral("48"));
    query.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mp157-ivi"));
    req.setTransferTimeout(8000);
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;
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

        m_hourlyTime.clear();
        m_hourlyCodes.clear();
        m_hourlyTemps.clear();
        m_hourlyDay.clear();
        m_hourlyVis.clear();
        m_hourlyWind.clear();
        m_hourlyGust.clear();
        m_hourlyPrecip.clear();
        m_hourlySoil.clear();

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
                now.precip = 0.0;
                if (!m_hourlyPrecip.isEmpty())
                    now.precip = m_hourlyPrecip.first();
                if (!m_hourlySoil.isEmpty())
                    now.soil = m_hourlySoil.first();
                applyLive(now, 0.0);
            }
        }
    });
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
                                         index,
                                         frac);
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

    emit updated();
}

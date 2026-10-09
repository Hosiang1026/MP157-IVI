#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

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
    Q_PROPERTY(bool located READ located NOTIFY updated)
public:
    explicit WeatherService(QObject *parent = nullptr);

    QString place() const;
    int temperature() const;
    QString condition() const;
    QString kind() const;
    bool day() const;
    QString preview() const;
    QString travelAlert() const;
    int windKmh() const;
    int visibilityM() const;
    bool located() const;

    Q_INVOKABLE void setPreview(const QString &mode);

signals:
    void updated();

private:
    struct LiveHour {
        int code = 0;
        int temp = 0;
        int day = 1;
        double visibility = 10000.0;
        double wind = 0.0;
        double gust = 0.0;
        double precip = 0.0;
        double soil = 0.0;
    };

    void fetchLocation();
    void fetchLocationFallback();
    void fetchPlace();
    void fetchWeather();
    void fetchWarnings();
    void applyLive(const LiveHour &now, double precipPrev2h);
    void updateFromHourly();
    static void decodeCode(int code, bool day, QString *kind, QString *condition);
    static void resolveDrivingKind(const LiveHour &now,
                                   double precipPrev2h,
                                   const QString &warning,
                                   QString *kind,
                                   QString *condition);
    static bool isRainKind(const QString &kind);
    static bool isSnowKind(const QString &kind);
    static bool warningIsTyphoon(const QString &warning);
    static LiveHour sampleHourly(const QVector<QDateTime> &times,
                                 const QVector<int> &codes,
                                 const QVector<int> &temps,
                                 const QVector<int> &days,
                                 const QVector<double> &vis,
                                 const QVector<double> &wind,
                                 const QVector<double> &gust,
                                 const QVector<double> &precip,
                                 const QVector<double> &soil,
                                 int index,
                                 double frac);

    QNetworkAccessManager *m_net = nullptr;
    QTimer m_timer;
    QTimer m_liveTimer;
    QVector<QDateTime> m_hourlyTime;
    QVector<int> m_hourlyCodes;
    QVector<int> m_hourlyTemps;
    QVector<int> m_hourlyDay;
    QVector<double> m_hourlyVis;
    QVector<double> m_hourlyWind;
    QVector<double> m_hourlyGust;
    QVector<double> m_hourlyPrecip;
    QVector<double> m_hourlySoil;
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
    bool m_located = false;
};

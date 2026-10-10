#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class NavSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString text READ text NOTIFY stepChanged)
    Q_PROPERTY(QString turn READ turn NOTIFY stepChanged)
    Q_PROPERTY(int speedLimit READ speedLimit NOTIFY stepChanged)
    Q_PROPERTY(QString destination READ destination NOTIFY stepChanged)
    Q_PROPERTY(int etaMin READ etaMin NOTIFY stepChanged)
    Q_PROPERTY(int distanceM READ distanceM NOTIFY stepChanged)
public:
    explicit NavSession(QObject *parent = nullptr);

    bool active() const;
    QString text() const;
    QString turn() const;
    int speedLimit() const;
    QString destination() const;
    int etaMin() const;
    int distanceM() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void startTo(const QString &destination);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void applyRemote(bool active, const QString &text, const QString &turn, int speedLimit,
                                 int etaMin, const QString &destination);

signals:
    void activeChanged();
    void stepChanged();

private:
    void advance();

    bool m_active = false;
    bool m_remote = false;
    int m_index = 0;
    int m_etaMin = 12;
    int m_speedLimit = 0;
    int m_distanceM = 0;
    QString m_destination;
    QString m_text;
    QString m_turn;
    QTimer m_timer;
};

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
public:
    explicit NavSession(QObject *parent = nullptr);

    bool active() const;
    QString text() const;
    QString turn() const;
    int speedLimit() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

signals:
    void activeChanged();
    void stepChanged();

private:
    void advance();

    bool m_active = false;
    int m_index = 0;
    QTimer m_timer;
};

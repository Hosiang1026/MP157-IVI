#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class NotificationSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool statusBarEnabled READ statusBarEnabled WRITE setStatusBarEnabled NOTIFY statusBarEnabledChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString appName READ appName NOTIFY infoChanged)
    Q_PROPERTY(QString title READ title NOTIFY infoChanged)
    Q_PROPERTY(QString body READ body NOTIFY infoChanged)
    Q_PROPERTY(QString text READ text NOTIFY infoChanged)
public:
    explicit NotificationSession(QObject *parent = nullptr);

    bool statusBarEnabled() const;
    void setStatusBarEnabled(bool value);
    bool active() const;
    QString appName() const;
    QString title() const;
    QString body() const;
    QString text() const;

    Q_INVOKABLE void dismiss();
    Q_INVOKABLE void applyRemote(const QString &appName, const QString &title, const QString &body);

signals:
    void statusBarEnabledChanged();
    void activeChanged();
    void infoChanged();

private:
    void load();
    void save() const;
    void setActive(bool value);

    bool m_statusBarEnabled = true;
    bool m_active = false;
    QString m_appName;
    QString m_title;
    QString m_body;
    QTimer m_clearTimer;
};

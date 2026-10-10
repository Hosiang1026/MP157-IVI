#include "NotificationSession.hpp"

#include <QSettings>

NotificationSession::NotificationSession(QObject *parent)
    : QObject(parent)
{
    load();
    m_clearTimer.setSingleShot(true);
    connect(&m_clearTimer, &QTimer::timeout, this, &NotificationSession::dismiss);
}

bool NotificationSession::statusBarEnabled() const
{
    return m_statusBarEnabled;
}

void NotificationSession::setStatusBarEnabled(bool value)
{
    if (m_statusBarEnabled == value)
        return;
    m_statusBarEnabled = value;
    save();
    emit statusBarEnabledChanged();
    if (!m_statusBarEnabled)
        setActive(false);
}

bool NotificationSession::active() const
{
    return m_active;
}

QString NotificationSession::appName() const
{
    return m_appName;
}

QString NotificationSession::title() const
{
    return m_title;
}

QString NotificationSession::body() const
{
    return m_body;
}

QString NotificationSession::text() const
{
    if (!m_title.isEmpty() && !m_body.isEmpty()) {
        if (m_appName.isEmpty() || m_appName == m_title)
            return m_title + QStringLiteral(" · ") + m_body;
        return m_appName + QStringLiteral(" · ") + m_title + QStringLiteral("：") + m_body;
    }
    if (!m_body.isEmpty())
        return m_appName.isEmpty() ? m_body : (m_appName + QStringLiteral(" · ") + m_body);
    if (!m_title.isEmpty())
        return m_appName.isEmpty() || m_appName == m_title
                   ? m_title
                   : (m_appName + QStringLiteral(" · ") + m_title);
    return m_appName;
}

void NotificationSession::dismiss()
{
    m_clearTimer.stop();
    if (!m_active && m_appName.isEmpty() && m_title.isEmpty() && m_body.isEmpty())
        return;
    m_appName.clear();
    m_title.clear();
    m_body.clear();
    emit infoChanged();
    setActive(false);
}

void NotificationSession::applyRemote(const QString &appName, const QString &title, const QString &body)
{
    if (appName.trimmed().isEmpty() && title.trimmed().isEmpty() && body.trimmed().isEmpty()) {
        dismiss();
        return;
    }
    m_appName = appName.trimmed();
    m_title = title.trimmed();
    m_body = body.trimmed();
    emit infoChanged();
    if (!m_statusBarEnabled) {
        setActive(false);
        return;
    }
    setActive(true);
    m_clearTimer.start(8000);
}

void NotificationSession::load()
{
    m_statusBarEnabled = QSettings().value(QStringLiteral("notification/statusBarEnabled"), true).toBool();
}

void NotificationSession::save() const
{
    QSettings().setValue(QStringLiteral("notification/statusBarEnabled"), m_statusBarEnabled);
}

void NotificationSession::setActive(bool value)
{
    if (m_active == value)
        return;
    m_active = value;
    emit activeChanged();
}

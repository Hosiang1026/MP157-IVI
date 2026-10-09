#include "CallSession.hpp"

#include "AudioFocus.hpp"

#include <QVariantMap>

namespace {

QVariantMap person(const QString &name, const QString &number)
{
    QVariantMap map;
    map.insert(QStringLiteral("name"), name);
    map.insert(QStringLiteral("number"), number);
    return map;
}

}

CallSession::CallSession(AudioFocus *audio, QObject *parent)
    : QObject(parent)
    , m_audio(audio)
{
    m_contacts = {
        person(QStringLiteral("张伟"), QStringLiteral("13800001111")),
        person(QStringLiteral("李娜"), QStringLiteral("13900002222")),
        person(QStringLiteral("王强"), QStringLiteral("13700003333")),
        person(QStringLiteral("赵敏"), QStringLiteral("13600004444")),
        person(QStringLiteral("服务中心"), QStringLiteral("4008001234"))
    };
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (!m_active)
            return;
        ++m_elapsed;
        emit elapsedChanged();
    });
}

bool CallSession::active() const
{
    return m_active;
}

QString CallSession::number() const
{
    return m_number;
}

QString CallSession::contactName() const
{
    return m_name;
}

bool CallSession::muted() const
{
    return m_muted;
}

bool CallSession::speaker() const
{
    return m_speaker;
}

int CallSession::elapsed() const
{
    return m_elapsed;
}

QVariantList CallSession::contacts() const
{
    return m_contacts;
}

QVariantList CallSession::recents() const
{
    return m_recents;
}

void CallSession::dial()
{
    dialNumber(m_number.isEmpty() ? QStringLiteral("13800001111") : m_number);
}

void CallSession::dialNumber(const QString &number)
{
    if (m_active || number.isEmpty())
        return;
    m_number = number;
    m_name = lookup(number);
    m_elapsed = 0;
    m_muted = false;
    m_speaker = true;
    m_recents.prepend(person(m_name.isEmpty() ? number : m_name, number));
    if (m_recents.size() > 8)
        m_recents.removeLast();
    m_active = true;
    emit infoChanged();
    emit elapsedChanged();
    emit mutedChanged();
    emit speakerChanged();
    emit recentsChanged();
    emit activeChanged();
    m_audio->request(QStringLiteral("call"), m_audio->callPriority());
    m_timer.start();
}

void CallSession::hangup()
{
    if (!m_active)
        return;
    m_active = false;
    m_timer.stop();
    emit activeChanged();
    m_audio->release(QStringLiteral("call"));
}

void CallSession::toggleMuted()
{
    if (!m_active)
        return;
    m_muted = !m_muted;
    emit mutedChanged();
}

void CallSession::toggleSpeaker()
{
    if (!m_active)
        return;
    m_speaker = !m_speaker;
    emit speakerChanged();
}

QString CallSession::lookup(const QString &number) const
{
    for (const QVariant &item : m_contacts) {
        const QVariantMap map = item.toMap();
        if (map.value(QStringLiteral("number")).toString() == number)
            return map.value(QStringLiteral("name")).toString();
    }
    return {};
}

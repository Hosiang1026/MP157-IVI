#include "CallSession.hpp"

#include "AudioFocus.hpp"

#include <QSettings>
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
    loadRecents();
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (!m_active)
            return;
        ++m_elapsed;
        emit elapsedChanged();
    });
    m_ringTimer.setSingleShot(true);
    connect(&m_ringTimer, &QTimer::timeout, this, &CallSession::connectCall);
}

bool CallSession::active() const
{
    return m_active;
}

bool CallSession::ringing() const
{
    return m_ringing;
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

void CallSession::loadRecents()
{
    const QVariantList saved = QSettings().value(QStringLiteral("phone/recents")).toList();
    m_recents.clear();
    for (const QVariant &item : saved) {
        const QVariantMap map = item.toMap();
        if (map.value(QStringLiteral("number")).toString().isEmpty())
            continue;
        m_recents.push_back(map);
        if (m_recents.size() >= 8)
            break;
    }
}

void CallSession::saveRecents() const
{
    QSettings().setValue(QStringLiteral("phone/recents"), m_recents);
}

void CallSession::dial()
{
    dialNumber(m_number.isEmpty() ? QStringLiteral("13800001111") : m_number);
}

void CallSession::dialNumber(const QString &number)
{
    if (m_active || m_ringing || number.isEmpty())
        return;
    m_number = number;
    m_name = lookup(number);
    m_elapsed = 0;
    m_muted = false;
    m_speaker = true;
    m_recents.prepend(person(m_name.isEmpty() ? number : m_name, number));
    if (m_recents.size() > 8)
        m_recents.removeLast();
    saveRecents();
    m_ringing = true;
    emit infoChanged();
    emit elapsedChanged();
    emit mutedChanged();
    emit speakerChanged();
    emit recentsChanged();
    emit ringingChanged();
    m_audio->request(QStringLiteral("call"), m_audio->callPriority());
    m_ringTimer.start(2000);
}

void CallSession::connectCall()
{
    if (!m_ringing)
        return;
    m_ringing = false;
    m_active = true;
    emit ringingChanged();
    emit activeChanged();
    m_timer.start();
}

void CallSession::answer()
{
    if (!m_ringing)
        return;
    m_ringTimer.stop();
    connectCall();
}

void CallSession::hangup()
{
    m_ringTimer.stop();
    const bool wasRinging = m_ringing;
    const bool wasActive = m_active;
    m_ringing = false;
    m_active = false;
    m_timer.stop();
    if (wasRinging)
        emit ringingChanged();
    if (wasActive)
        emit activeChanged();
    if (wasRinging || wasActive)
        m_audio->release(QStringLiteral("call"));
}

void CallSession::applyRemote(bool active, bool ringing, const QString &name, const QString &number)
{
    m_ringTimer.stop();
    const bool wasRinging = m_ringing;
    const bool wasActive = m_active;
    m_ringing = ringing;
    m_active = active && !ringing;
    if (!name.isEmpty())
        m_name = name;
    if (!number.isEmpty())
        m_number = number;
    if (m_active && !wasActive) {
        m_elapsed = 0;
        m_timer.start();
        if (m_audio)
            m_audio->request(QStringLiteral("call"), m_audio->callPriority());
    }
    if (!m_active && !m_ringing) {
        m_timer.stop();
        if (wasActive || wasRinging) {
            if (m_audio)
                m_audio->release(QStringLiteral("call"));
        }
    }
    emit infoChanged();
    if (wasRinging != m_ringing)
        emit ringingChanged();
    if (wasActive != m_active)
        emit activeChanged();
    emit elapsedChanged();
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

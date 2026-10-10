#include "NavSession.hpp"

#include <QtGlobal>

namespace {

struct Step {
    const char *turn;
    const char *text;
    int speedLimit;
    int distanceM;
};

const Step kSteps[] = {
    {"straight", "沿当前道路直行 800 米", 60, 800},
    {"right", "300 米后右转", 40, 300},
    {"left", "前方 500 米左转", 50, 500},
    {"straight", "保持直行 1.2 公里", 80, 1200},
    {"arrive", "目的地在右侧", 30, 40},
};

}

NavSession::NavSession(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(3500);
    connect(&m_timer, &QTimer::timeout, this, &NavSession::advance);
}

bool NavSession::active() const
{
    return m_active;
}

QString NavSession::text() const
{
    if (!m_active)
        return {};
    if (m_remote)
        return m_text;
    QString line = QString::fromUtf8(kSteps[m_index].text);
    if (m_index == int(sizeof(kSteps) / sizeof(kSteps[0])) - 1 && !m_destination.isEmpty())
        line = QStringLiteral("到达「%1」").arg(m_destination);
    return line;
}

QString NavSession::turn() const
{
    if (!m_active)
        return {};
    if (m_remote)
        return m_turn;
    return QString::fromUtf8(kSteps[m_index].turn);
}

int NavSession::speedLimit() const
{
    if (!m_active)
        return 0;
    if (m_remote)
        return m_speedLimit;
    return kSteps[m_index].speedLimit;
}

QString NavSession::destination() const
{
    return m_destination;
}

int NavSession::etaMin() const
{
    return m_active ? m_etaMin : 0;
}

int NavSession::distanceM() const
{
    if (!m_active)
        return 0;
    if (m_remote)
        return m_distanceM;
    return kSteps[m_index].distanceM;
}

void NavSession::start()
{
    startTo(m_destination.isEmpty() ? QStringLiteral("目的地") : m_destination);
}

void NavSession::startTo(const QString &destination)
{
    m_remote = false;
    m_destination = destination.trimmed().isEmpty() ? QStringLiteral("目的地") : destination.trimmed();
    m_index = 0;
    m_etaMin = 12;
    m_distanceM = kSteps[0].distanceM;
    if (!m_active) {
        m_active = true;
        emit activeChanged();
    }
    emit stepChanged();
    m_timer.start();
}

void NavSession::stop()
{
    m_timer.stop();
    m_remote = false;
    m_distanceM = 0;
    if (!m_active)
        return;
    m_active = false;
    emit activeChanged();
    emit stepChanged();
}

void NavSession::applyRemote(bool active, const QString &text, const QString &turn, int speedLimit,
                             int etaMin, const QString &destination)
{
    m_timer.stop();
    m_remote = true;
    m_text = text;
    m_turn = turn;
    m_speedLimit = qMax(0, speedLimit);
    m_etaMin = qMax(0, etaMin);
    m_distanceM = 0;
    if (!destination.isEmpty())
        m_destination = destination;
    if (m_active != active) {
        m_active = active;
        emit activeChanged();
    }
    emit stepChanged();
}

void NavSession::advance()
{
    const int count = int(sizeof(kSteps) / sizeof(kSteps[0]));
    if (m_index + 1 >= count) {
        stop();
        return;
    }
    ++m_index;
    m_etaMin = qMax(1, m_etaMin - 2);
    m_distanceM = kSteps[m_index].distanceM;
    emit stepChanged();
}

#include "NavSession.hpp"

namespace {

struct Step {
    const char *turn;
    const char *text;
    int speedLimit;
};

const Step kSteps[] = {
    {"straight", "沿当前道路直行 800 米", 60},
    {"right", "300 米后右转", 40},
    {"left", "前方 500 米左转", 50},
    {"straight", "保持直行 1.2 公里", 80},
    {"arrive", "目的地在右侧", 30},
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
    return QString::fromUtf8(kSteps[m_index].text);
}

QString NavSession::turn() const
{
    if (!m_active)
        return {};
    return QString::fromUtf8(kSteps[m_index].turn);
}

int NavSession::speedLimit() const
{
    if (!m_active)
        return 0;
    return kSteps[m_index].speedLimit;
}

void NavSession::start()
{
    m_index = 0;
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
    if (!m_active)
        return;
    m_active = false;
    emit activeChanged();
    emit stepChanged();
}

void NavSession::advance()
{
    const int count = int(sizeof(kSteps) / sizeof(kSteps[0]));
    m_index = (m_index + 1) % count;
    emit stepChanged();
}

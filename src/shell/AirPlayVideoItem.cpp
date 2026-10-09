#include "AirPlayVideoItem.hpp"

#include <QPainter>

AirPlayVideoItem::AirPlayVideoItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setOpaquePainting(true);
}

void AirPlayVideoItem::setSession(AirPlayMirrorSession *session)
{
    if (m_session == session)
        return;
    if (m_session)
        disconnect(m_session, nullptr, this, nullptr);
    m_session = session;
    if (m_session)
        connect(m_session, &AirPlayMirrorSession::videoFrameChanged, this, &AirPlayVideoItem::onFrame,
                Qt::QueuedConnection);
    emit sessionChanged();
    onFrame();
}

void AirPlayVideoItem::onFrame()
{
    if (!m_session)
        return;
    m_frame = m_session->videoFrame();
    update();
}

void AirPlayVideoItem::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), Qt::black);
    if (m_frame.isNull())
        return;
    painter->drawImage(boundingRect(), m_frame);
}

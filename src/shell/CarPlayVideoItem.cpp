#include "CarPlayVideoItem.hpp"

#include "CarPlaySession.hpp"

#include <QPainter>

CarPlayVideoItem::CarPlayVideoItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setMipmap(false);
    setOpaquePainting(true);
    setAntialiasing(false);
}

void CarPlayVideoItem::setSession(CarPlaySession *session)
{
    if (m_session == session)
        return;
    if (m_session)
        disconnect(m_session, nullptr, this, nullptr);
    m_session = session;
    if (m_session)
        connect(m_session, &CarPlaySession::videoFrameChanged, this, &CarPlayVideoItem::onFrame,
                Qt::QueuedConnection);
    emit sessionChanged();
    onFrame();
}

void CarPlayVideoItem::refreshContentRect()
{
    const QRectF next = (!m_frame.isNull() && width() > 0 && height() > 0)
        ? QRectF(0, 0, width(), height())
        : QRectF();
    if (next != m_content) {
        m_content = next;
        emit contentRectChanged();
    }
}

void CarPlayVideoItem::onFrame()
{
    if (!m_session)
        return;
    m_frame = m_session->videoFrame();
    refreshContentRect();
    update();
}

void CarPlayVideoItem::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), Qt::black);
    refreshContentRect();
    if (m_frame.isNull() || m_content.isEmpty())
        return;
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter->drawImage(m_content, m_frame);
}

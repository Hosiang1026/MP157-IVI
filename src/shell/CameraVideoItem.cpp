#include "CameraVideoItem.hpp"

#include <QPainter>

CameraVideoItem::CameraVideoItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setMipmap(false);
    setOpaquePainting(true);
    setAntialiasing(false);
}

void CameraVideoItem::setSession(CameraService *session)
{
    if (m_session == session)
        return;
    if (m_session)
        disconnect(m_session, nullptr, this, nullptr);
    m_session = session;
    if (m_session)
        connect(m_session, &CameraService::frameChanged, this, &CameraVideoItem::onFrame, Qt::QueuedConnection);
    emit sessionChanged();
    onFrame();
}

void CameraVideoItem::onFrame()
{
    if (!m_session)
        return;
    m_frame = m_session->currentFrame();
    update();
}

void CameraVideoItem::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), Qt::black);
    if (m_frame.isNull() || width() <= 0 || height() <= 0)
        return;
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    const QSizeF box(width(), height());
    QSizeF sz = m_frame.size();
    sz.scale(box, Qt::KeepAspectRatioByExpanding);
    const QRectF target((box.width() - sz.width()) * 0.5, (box.height() - sz.height()) * 0.5, sz.width(),
                        sz.height());
    painter->drawImage(target, m_frame);
}

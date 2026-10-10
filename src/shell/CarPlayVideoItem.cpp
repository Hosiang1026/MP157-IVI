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
    setPerformanceHint(QQuickPaintedItem::FastFBOResizing, true);
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

void CarPlayVideoItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        refreshContentRect();
        update();
    }
}

void CarPlayVideoItem::refreshContentRect()
{
    const bool hasFrame = !snapshotFrame().isNull();
    const QRectF next = (hasFrame && width() > 0 && height() > 0) ? QRectF(0, 0, width(), height())
                                                                  : QRectF();
    if (next != m_content) {
        m_content = next;
        emit contentRectChanged();
    }
}

QImage CarPlayVideoItem::snapshotFrame() const
{
    QMutexLocker lock(&m_mutex);
    return m_frame;
}

void CarPlayVideoItem::onFrame()
{
    if (!m_session)
        return;
    QImage frame = m_session->videoFrame();
    if (frame.isNull())
        return;
    if (frame.format() != QImage::Format_RGB32 && frame.format() != QImage::Format_ARGB32
        && frame.format() != QImage::Format_ARGB32_Premultiplied)
        frame = frame.convertToFormat(QImage::Format_RGB32);
    {
        QMutexLocker lock(&m_mutex);
        m_frame = frame;
    }
    refreshContentRect();
    update();
}

void CarPlayVideoItem::paint(QPainter *painter)
{
    const QImage frame = snapshotFrame();
    const QRectF dest = (!frame.isNull() && width() > 0 && height() > 0)
        ? QRectF(0, 0, width(), height())
        : QRectF();
    painter->fillRect(boundingRect(), Qt::black);
    if (frame.isNull() || dest.isEmpty())
        return;
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter->drawImage(dest, frame);
}

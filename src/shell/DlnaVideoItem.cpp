#include "DlnaVideoItem.hpp"

#include <QPainter>

DlnaVideoItem::DlnaVideoItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setOpaquePainting(true);
}

void DlnaVideoItem::setSession(DlnaRenderer *session)
{
    if (m_session == session)
        return;
    if (m_session)
        disconnect(m_session, nullptr, this, nullptr);
    m_session = session;
    if (m_session)
        connect(m_session, &DlnaRenderer::frameChanged, this, &DlnaVideoItem::onFrame, Qt::QueuedConnection);
    emit sessionChanged();
    onFrame();
}

void DlnaVideoItem::onFrame()
{
    if (!m_session)
        return;
    m_frame = m_session->currentImage();
    update();
}

void DlnaVideoItem::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), Qt::black);
    if (m_frame.isNull())
        return;
    const QSizeF fitted = QSizeF(m_frame.size()).scaled(boundingRect().size(), Qt::KeepAspectRatio);
    const QRectF target((width() - fitted.width()) / 2.0, (height() - fitted.height()) / 2.0, fitted.width(),
                        fitted.height());
    painter->drawImage(target, m_frame);
}

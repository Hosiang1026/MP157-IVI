#pragma once

#include "CarPlaySession.hpp"

#include <QImage>
#include <QQuickPaintedItem>

class CarPlayVideoItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(CarPlaySession *session READ session WRITE setSession NOTIFY sessionChanged)
    Q_PROPERTY(qreal contentX READ contentX NOTIFY contentRectChanged)
    Q_PROPERTY(qreal contentY READ contentY NOTIFY contentRectChanged)
    Q_PROPERTY(qreal contentWidth READ contentWidth NOTIFY contentRectChanged)
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY contentRectChanged)
public:
    explicit CarPlayVideoItem(QQuickItem *parent = nullptr);

    CarPlaySession *session() const { return m_session; }
    void setSession(CarPlaySession *session);
    qreal contentX() const { return m_content.x(); }
    qreal contentY() const { return m_content.y(); }
    qreal contentWidth() const { return m_content.width(); }
    qreal contentHeight() const { return m_content.height(); }

    void paint(QPainter *painter) override;

signals:
    void sessionChanged();
    void contentRectChanged();

private slots:
    void onFrame();

private:
    void refreshContentRect();

    CarPlaySession *m_session = nullptr;
    QImage m_frame;
    QRectF m_content;
};

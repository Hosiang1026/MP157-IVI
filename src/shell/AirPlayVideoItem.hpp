#pragma once

#include "AirPlayMirrorSession.hpp"

#include <QImage>
#include <QQuickPaintedItem>

class AirPlayVideoItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(AirPlayMirrorSession *session READ session WRITE setSession NOTIFY sessionChanged)
public:
    explicit AirPlayVideoItem(QQuickItem *parent = nullptr);

    AirPlayMirrorSession *session() const { return m_session; }
    void setSession(AirPlayMirrorSession *session);

    void paint(QPainter *painter) override;

signals:
    void sessionChanged();

private slots:
    void onFrame();

private:
    AirPlayMirrorSession *m_session = nullptr;
    QImage m_frame;
};

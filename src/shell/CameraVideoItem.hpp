#pragma once

#include "CameraService.hpp"

#include <QImage>
#include <QQuickPaintedItem>

class CameraVideoItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(CameraService *session READ session WRITE setSession NOTIFY sessionChanged)
public:
    explicit CameraVideoItem(QQuickItem *parent = nullptr);

    CameraService *session() const { return m_session; }
    void setSession(CameraService *session);

    void paint(QPainter *painter) override;

signals:
    void sessionChanged();

private slots:
    void onFrame();

private:
    CameraService *m_session = nullptr;
    QImage m_frame;
};

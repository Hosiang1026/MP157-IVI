#pragma once

#include "DlnaRenderer.hpp"

#include <QImage>
#include <QQuickPaintedItem>

class DlnaVideoItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(DlnaRenderer *session READ session WRITE setSession NOTIFY sessionChanged)
public:
    explicit DlnaVideoItem(QQuickItem *parent = nullptr);

    DlnaRenderer *session() const { return m_session; }
    void setSession(DlnaRenderer *session);

    void paint(QPainter *painter) override;

signals:
    void sessionChanged();

private slots:
    void onFrame();

private:
    DlnaRenderer *m_session = nullptr;
    QImage m_frame;
};

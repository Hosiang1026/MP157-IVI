#include "VideoScreen.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QtEndian>

VideoScreen::VideoScreen(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setFillColor(Qt::black);
    m_timer.setInterval(125);
    connect(&m_timer, &QTimer::timeout, this, &VideoScreen::tick);
}

QString VideoScreen::clipName() const
{
    return m_clip;
}

void VideoScreen::setClipName(const QString &name)
{
    if (m_clip == name && !m_frames.isEmpty())
        return;
    m_clip = name;
    const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("media/video/") + name + QStringLiteral(".avi"));
    load(path);
    m_frame = 0;
    m_position = 0;
    m_timer.setInterval(qMax(1, 1000 / qMax(1, m_fps)));
    emit clipNameChanged();
    emit durationChanged();
    emit positionChanged();
    update();
}

bool VideoScreen::playing() const
{
    return m_playing;
}

void VideoScreen::setPlaying(bool value)
{
    if (m_playing == value)
        return;
    m_playing = value;
    if (m_playing && !m_frames.isEmpty() && m_frame >= m_frames.size() - 1)
        m_frame = 0;
    if (m_playing)
        m_timer.start();
    else
        m_timer.stop();
    emit playingChanged();
    update();
}

int VideoScreen::position() const
{
    return m_position;
}

int VideoScreen::duration() const
{
    if (m_frames.isEmpty() || m_fps <= 0)
        return 0;
    return qMax(1, m_frames.size() / m_fps);
}

void VideoScreen::seek(int seconds)
{
    if (m_frames.isEmpty() || m_fps <= 0)
        return;
    m_frame = qBound(0, seconds * m_fps, m_frames.size() - 1);
    const int next = m_frame / m_fps;
    if (next != m_position) {
        m_position = next;
        emit positionChanged();
    }
    update();
}

void VideoScreen::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), Qt::black);
    if (m_frames.isEmpty() || m_frame < 0 || m_frame >= m_frames.size())
        return;
    const QImage &frame = m_frames.at(m_frame);
    const QRectF area = boundingRect();
    const QSizeF fitted = frame.size().scaled(area.size().toSize(), Qt::KeepAspectRatio);
    const QRectF target(area.x() + (area.width() - fitted.width()) / 2.0,
                        area.y() + (area.height() - fitted.height()) / 2.0,
                        fitted.width(),
                        fitted.height());
    painter->drawImage(target, frame);
}

void VideoScreen::load(const QString &path)
{
    m_frames.clear();
    m_fps = 8;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QByteArray data = file.readAll();
    const int strf = data.indexOf("strf");
    const int avih = data.indexOf("avih");
    const int movi = data.indexOf("movi");
    if (strf < 0 || movi < 0 || strf + 20 > data.size())
        return;
    const int width = qFromLittleEndian<qint32>(reinterpret_cast<const uchar *>(data.constData() + strf + 12));
    const int height = qFromLittleEndian<qint32>(reinterpret_cast<const uchar *>(data.constData() + strf + 16));
    if (avih >= 0 && avih + 12 <= data.size()) {
        const quint32 usec = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + avih + 8));
        if (usec > 0)
            m_fps = qMax(1, int(1000000 / usec));
    }
    const int absH = qAbs(height);
    if (width <= 0 || absH <= 0)
        return;
    const int stride = (width * 3 + 3) & ~3;
    int i = movi + 4;
    while (i + 8 <= data.size()) {
        const QByteArray id = data.mid(i, 4);
        if (id == "idx1")
            break;
        const quint32 size = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + i + 4));
        if (i + 8 + int(size) > data.size())
            break;
        if (id == "00db" && int(size) >= stride * absH) {
            QImage image(width, absH, QImage::Format_RGB32);
            const char *bytes = data.constData() + i + 8;
            for (int y = 0; y < absH; ++y) {
                const int srcY = height > 0 ? absH - 1 - y : y;
                const uchar *row = reinterpret_cast<const uchar *>(bytes + srcY * stride);
                auto *dst = reinterpret_cast<QRgb *>(image.scanLine(y));
                for (int x = 0; x < width; ++x)
                    dst[x] = qRgb(row[x * 3 + 2], row[x * 3 + 1], row[x * 3]);
            }
            m_frames.push_back(image);
        }
        i += 8 + int(size) + int(size & 1);
    }
}

void VideoScreen::tick()
{
    if (!m_playing || m_frames.isEmpty())
        return;
    if (m_frame + 1 >= m_frames.size()) {
        setPlaying(false);
        return;
    }
    ++m_frame;
    const int seconds = m_frame / qMax(1, m_fps);
    if (seconds != m_position) {
        m_position = seconds;
        emit positionChanged();
    }
    update();
}

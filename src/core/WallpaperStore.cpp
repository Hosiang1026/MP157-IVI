#include "WallpaperStore.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUrl>

namespace {

QString fileUrl(const QString &path)
{
    return QUrl::fromLocalFile(path).toString();
}

bool isImageSuffix(const QString &suffix)
{
    const QString s = suffix.toLower();
    return s == QStringLiteral("jpg") || s == QStringLiteral("jpeg") || s == QStringLiteral("png")
           || s == QStringLiteral("webp") || s == QStringLiteral("bmp") || s == QStringLiteral("gif");
}

}

WallpaperStore::WallpaperStore(QObject *parent)
    : QObject(parent)
{
    const QString root = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("wallpapers"));
    m_builtinDir = QDir(root).filePath(QStringLiteral("builtin"));
    m_userDir = QDir(root).filePath(QStringLiteral("user"));
    QDir().mkpath(m_userDir);
    reload();

    const QString saved = QSettings().value(QStringLiteral("wallpaper")).toString();
    if (!saved.isEmpty() && QFile::exists(saved))
        m_current = fileUrl(saved);
    else if (!m_items.isEmpty())
        m_current = m_items.first().toMap().value(QStringLiteral("path")).toString();
    refreshBackdrop();
}

QString WallpaperStore::current() const
{
    return m_current;
}

QVariantList WallpaperStore::items() const
{
    return m_items;
}

bool WallpaperStore::darkBackdrop() const
{
    return m_darkBackdrop;
}

void WallpaperStore::select(const QString &path)
{
    const QString local = QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path;
    if (!QFile::exists(local))
        return;
    setCurrent(local);
}

QString WallpaperStore::picturesDir() const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("media/pictures"));
}

QVariantList WallpaperStore::pickableImages() const
{
    QVariantList list;
    const QStringList roots = {
        picturesDir(),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("media/inbox")),
    };
    QStringList filters = {
        QStringLiteral("*.jpg"),  QStringLiteral("*.jpeg"), QStringLiteral("*.png"),
        QStringLiteral("*.webp"), QStringLiteral("*.bmp"),  QStringLiteral("*.gif"),
        QStringLiteral("*.JPG"),  QStringLiteral("*.JPEG"), QStringLiteral("*.PNG"),
        QStringLiteral("*.WEBP"), QStringLiteral("*.BMP"),  QStringLiteral("*.GIF"),
    };
    for (const QString &root : roots) {
        QDir dir(root);
        if (!dir.exists())
            continue;
        const QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);
        for (const QFileInfo &info : files) {
            if (!isImageSuffix(info.suffix()))
                continue;
            QVariantMap item;
            item.insert(QStringLiteral("name"), info.fileName());
            item.insert(QStringLiteral("path"), info.absoluteFilePath());
            item.insert(QStringLiteral("url"), fileUrl(info.absoluteFilePath()));
            list.push_back(item);
        }
    }
    return list;
}

bool WallpaperStore::importFrom(const QString &path)
{
    const QString local = QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path;
    return copyIn(local);
}

bool WallpaperStore::removeCustom(const QString &path)
{
    const QString local = QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path;
    const QFileInfo info(local);
    if (!info.exists() || !info.isFile())
        return false;
    const QString relative = QDir(m_userDir).relativeFilePath(info.absoluteFilePath());
    if (relative.startsWith(QLatin1String("..")) || QFileInfo(relative).isAbsolute())
        return false;
    if (!QFile::remove(info.absoluteFilePath()))
        return false;
    const bool wasCurrent = m_current == fileUrl(info.absoluteFilePath());
    reload();
    if (wasCurrent) {
        if (!m_items.isEmpty())
            setCurrent(QUrl(m_items.first().toMap().value(QStringLiteral("path")).toString()).toLocalFile());
        else {
            m_current.clear();
            QSettings().remove(QStringLiteral("wallpaper"));
            emit currentChanged();
            refreshBackdrop();
        }
    }
    return true;
}

void WallpaperStore::reload()
{
    QVariantList list;
    const QString catalogPath = QDir(m_builtinDir).filePath(QStringLiteral("catalog.json"));
    QFile catalogFile(catalogPath);
    if (catalogFile.open(QIODevice::ReadOnly)) {
        const QJsonArray array = QJsonDocument::fromJson(catalogFile.readAll()).array();
        for (const QJsonValue &value : array) {
            const QJsonObject obj = value.toObject();
            const QString filePath = QDir(m_builtinDir).filePath(obj.value(QStringLiteral("file")).toString());
            if (!QFile::exists(filePath))
                continue;
            QVariantMap item;
            item.insert(QStringLiteral("name"), obj.value(QStringLiteral("name")).toString());
            item.insert(QStringLiteral("path"), fileUrl(filePath));
            item.insert(QStringLiteral("custom"), false);
            list.push_back(item);
        }
    }

    const QFileInfoList customFiles = QDir(m_userDir).entryInfoList(
        {QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png"), QStringLiteral("*.webp"),
         QStringLiteral("*.bmp"), QStringLiteral("*.gif")},
        QDir::Files, QDir::Time);
    int index = 1;
    for (const QFileInfo &info : customFiles) {
        QVariantMap item;
        item.insert(QStringLiteral("name"), QStringLiteral("上传 %1").arg(index++));
        item.insert(QStringLiteral("path"), fileUrl(info.absoluteFilePath()));
        item.insert(QStringLiteral("custom"), true);
        list.push_back(item);
    }

    m_items = list;
    emit itemsChanged();
}

void WallpaperStore::setCurrent(const QString &filePath)
{
    const QString url = fileUrl(filePath);
    QSettings().setValue(QStringLiteral("wallpaper"), filePath);
    if (url == m_current)
        return;
    m_current = url;
    emit currentChanged();
    refreshBackdrop();
}

void WallpaperStore::refreshBackdrop()
{
    bool dark = true;
    const QString local = QUrl(m_current).isLocalFile() ? QUrl(m_current).toLocalFile() : m_current;
    QImage image(local);
    if (!image.isNull()) {
        const QImage sample = image.scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                  .convertToFormat(QImage::Format_RGB32);
        qint64 sum = 0;
        int count = 0;
        const int y0 = sample.height() * 2 / 3;
        for (int y = y0; y < sample.height(); ++y) {
            const QRgb *line = reinterpret_cast<const QRgb *>(sample.constScanLine(y));
            for (int x = 0; x < sample.width(); ++x) {
                const QRgb c = line[x];
                sum += qRed(c) * 299 + qGreen(c) * 587 + qBlue(c) * 114;
                ++count;
            }
        }
        if (count > 0)
            dark = (sum / count) < 140000;
    }
    if (m_darkBackdrop == dark)
        return;
    m_darkBackdrop = dark;
    emit darkBackdropChanged();
}

bool WallpaperStore::copyIn(const QString &sourcePath)
{
    const QString suffix = QFileInfo(sourcePath).suffix().toLower();
    if (!isImageSuffix(suffix))
        return false;
    const QString dest = QDir(m_userDir).filePath(
        QStringLiteral("upload-%1.%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(suffix));
    if (!QFile::copy(sourcePath, dest))
        return false;
    reload();
    setCurrent(dest);
    return true;
}

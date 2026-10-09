#include "WallpaperStore.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#include <commdlg.h>
#endif

namespace {

QString fileUrl(const QString &path)
{
    return QUrl::fromLocalFile(path).toString();
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
}

QString WallpaperStore::current() const
{
    return m_current;
}

QVariantList WallpaperStore::items() const
{
    return m_items;
}

void WallpaperStore::select(const QString &path)
{
    const QString local = QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path;
    if (!QFile::exists(local))
        return;
    setCurrent(local);
}

void WallpaperStore::upload()
{
#ifdef Q_OS_WIN
    wchar_t buffer[MAX_PATH] = {};
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter = L"Image\0*.jpg;*.jpeg;*.png;*.webp;*.bmp\0";
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (!GetOpenFileNameW(&dialog))
        return;
    copyIn(QString::fromWCharArray(buffer));
#else
    return;
#endif
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
        {QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png"), QStringLiteral("*.webp"), QStringLiteral("*.bmp")},
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
}

bool WallpaperStore::copyIn(const QString &sourcePath)
{
    const QString suffix = QFileInfo(sourcePath).suffix().toLower();
    if (suffix != QStringLiteral("jpg") && suffix != QStringLiteral("jpeg") && suffix != QStringLiteral("png")
        && suffix != QStringLiteral("webp") && suffix != QStringLiteral("bmp"))
        return false;
    const QString dest = QDir(m_userDir).filePath(
        QStringLiteral("upload-%1.%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(suffix));
    if (!QFile::copy(sourcePath, dest))
        return false;
    reload();
    setCurrent(dest);
    return true;
}

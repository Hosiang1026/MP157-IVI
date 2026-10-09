#include "AppCatalog.hpp"

#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QSortFilterProxyModel>

namespace {

class DockModel : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        return sourceModel()->index(row, 0, parent).data(AppListModel::DockRole).toBool();
    }

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        return left.data(AppListModel::DockOrderRole).toInt()
            < right.data(AppListModel::DockOrderRole).toInt();
    }
};

class UserAppModel : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        return !sourceModel()->index(row, 0, parent).data(AppListModel::BuiltinRole).toBool();
    }
};

bool validId(const QString &id)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_-]+$"));
    return re.match(id).hasMatch();
}

bool copyDir(const QString &fromPath, const QString &toPath)
{
    QDir from(fromPath);
    if (!from.exists())
        return false;
    if (!QDir().mkpath(toPath))
        return false;

    const auto entries = from.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo &entry : entries) {
        const QString target = QDir(toPath).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDir(entry.absoluteFilePath(), target))
                return false;
        } else if (!QFile::copy(entry.absoluteFilePath(), target)) {
            return false;
        }
    }
    return true;
}

}

AppCatalog::AppCatalog(QObject *parent)
    : QObject(parent)
    , m_root(detectRoot())
    , m_installed(new AppListModel(this))
    , m_available(new AppListModel(this))
{
    auto *dock = new DockModel(this);
    dock->setSourceModel(m_installed);
    dock->setDynamicSortFilter(true);
    dock->sort(0, Qt::AscendingOrder);
    m_dock = dock;

    auto *userApps = new UserAppModel(this);
    userApps->setSourceModel(m_installed);
    userApps->setDynamicSortFilter(true);
    m_userApps = userApps;

    reload();
}

QAbstractItemModel *AppCatalog::installed() const
{
    return m_installed;
}

QAbstractItemModel *AppCatalog::dock() const
{
    return m_dock;
}

QAbstractItemModel *AppCatalog::userApps() const
{
    return m_userApps;
}

QAbstractItemModel *AppCatalog::available() const
{
    return m_available;
}

bool AppCatalog::install(const QString &id)
{
    if (!validId(id) || m_installed->contains(id))
        return false;

    const QString src = QDir(m_root).filePath(QStringLiteral("feed/") + id);
    const QString dst = QDir(m_root).filePath(QStringLiteral("apps/") + id);
    if (!QDir(src).exists())
        return false;
    if (!copyDir(src, dst)) {
        QDir(dst).removeRecursively();
        return false;
    }
    reload();
    return true;
}

bool AppCatalog::uninstall(const QString &id)
{
    if (!validId(id) || !m_installed->contains(id) || m_installed->isBuiltin(id))
        return false;

    const QString dst = QDir(m_root).filePath(QStringLiteral("apps/") + id);
    if (!QDir(dst).removeRecursively())
        return false;
    reload();
    return true;
}

bool AppCatalog::addToDock(const QString &id)
{
    if (!m_installed->contains(id))
        return false;
    QStringList ids = dockIds();
    if (ids.contains(id))
        return true;
    if (ids.size() >= kMaxDock)
        return false;
    ids.append(id);
    saveDock(ids);
    reload();
    return true;
}

bool AppCatalog::moveDock(const QString &id, int index)
{
    if (!m_installed->contains(id))
        return false;
    QStringList ids = dockIds();
    const int from = ids.indexOf(id);
    if (from < 0 && ids.size() >= kMaxDock)
        return false;
    if (from >= 0)
        ids.removeAt(from);
    if (from >= 0 && from < index)
        --index;
    index = qBound(0, index, ids.size());
    ids.insert(index, id);
    saveDock(ids);
    reload();
    return true;
}

QVariantMap AppCatalog::appInfo(const QString &key) const
{
    return m_installed->info(key);
}

bool AppCatalog::moveHome(const QString &id, int index)
{
    if (!m_installed->contains(id))
        return false;
    QStringList ids = homeIds();
    const int from = ids.indexOf(id);
    if (from < 0)
        return false;
    ids.removeAt(from);
    if (from < index)
        --index;
    index = qBound(0, index, ids.size());
    ids.insert(index, id);
    QSettings().setValue(QStringLiteral("homeOrder"), ids);
    reload();
    return true;
}

bool AppCatalog::removeFromDock(const QString &id)
{
    QStringList ids = dockIds();
    if (!ids.removeAll(id))
        return false;
    saveDock(ids);
    reload();
    return true;
}

QStringList AppCatalog::dockIds() const
{
    QSettings settings;
    if (settings.contains(QStringLiteral("dock")))
        return settings.value(QStringLiteral("dock")).toStringList();
    return m_installed->dockIds();
}

QStringList AppCatalog::homeIds() const
{
    const QStringList current = m_installed->ids();
    QSettings settings;
    QStringList saved = settings.contains(QStringLiteral("homeOrder"))
        ? settings.value(QStringLiteral("homeOrder")).toStringList()
        : current;
    QStringList ids;
    for (const QString &id : saved) {
        if (current.contains(id) && !ids.contains(id))
            ids.append(id);
    }
    for (const QString &id : current) {
        if (!ids.contains(id))
            ids.append(id);
    }
    return ids;
}

void AppCatalog::saveDock(const QStringList &ids)
{
    QSettings().setValue(QStringLiteral("dock"), ids);
}

void AppCatalog::applyHome(QList<AppItem> &items) const
{
    QSettings settings;
    if (!settings.contains(QStringLiteral("homeOrder")))
        return;
    const QStringList ids = settings.value(QStringLiteral("homeOrder")).toStringList();
    for (AppItem &item : items) {
        const int index = ids.indexOf(item.id);
        if (index >= 0)
            item.order = index;
        else
            item.order += 1000;
    }
    std::sort(items.begin(), items.end(), [](const AppItem &a, const AppItem &b) {
        if (a.order != b.order)
            return a.order < b.order;
        return a.name < b.name;
    });
}

void AppCatalog::applyDock(QList<AppItem> &items) const
{
    QSettings settings;
    if (!settings.contains(QStringLiteral("dock")))
        return;
    const QStringList ids = settings.value(QStringLiteral("dock")).toStringList();
    for (AppItem &item : items) {
        const int index = ids.indexOf(item.id);
        item.dock = index >= 0;
        item.dockOrder = index >= 0 ? index : 100;
    }
}

void AppCatalog::reload()
{
    QList<AppItem> installed = AppListModel::readDirectory(QDir(m_root).filePath(QStringLiteral("apps")));
    applyHome(installed);
    applyDock(installed);
    QSet<QString> ids;
    for (const AppItem &item : installed)
        ids.insert(item.id);

    QList<AppItem> available;
    const QList<AppItem> feed = AppListModel::readDirectory(QDir(m_root).filePath(QStringLiteral("feed")));
    for (const AppItem &item : feed) {
        if (!ids.contains(item.id))
            available.push_back(item);
    }

    m_installed->setItems(installed);
    m_available->setItems(available);
}

QString AppCatalog::detectRoot() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir,
        QDir(appDir).filePath(QStringLiteral("../share/mp157-ivi")),
        QStringLiteral("/opt/ivi")
    };

    for (const QString &candidate : candidates) {
        const QString root = QDir(candidate).absolutePath();
        if (QDir(root + QStringLiteral("/apps")).exists())
            return root;
    }

    const QString fallback = QDir(appDir).absolutePath();
    QDir().mkpath(fallback + QStringLiteral("/apps"));
    QDir().mkpath(fallback + QStringLiteral("/feed"));
    return fallback;
}

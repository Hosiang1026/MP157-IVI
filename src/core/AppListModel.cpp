#include "AppListModel.hpp"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <algorithm>

AppListModel::AppListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AppListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

int AppListModel::count() const
{
    return m_items.size();
}

QVariant AppListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const AppItem &item = m_items.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return item.name;
    case IdRole:
        return item.id;
    case EntryRole:
        return item.entry;
    case IconRole:
        return item.icon;
    case ColorRole:
        return item.color;
    case BlurbRole:
        return item.blurb;
    case VersionRole:
        return item.version;
    case SizeKbRole:
        return item.sizeKb;
    case DockRole:
        return item.dock;
    case DockOrderRole:
        return item.dockOrder;
    case BuiltinRole:
        return item.builtin;
    default:
        return {};
    }
}

QHash<int, QByteArray> AppListModel::roleNames() const
{
    return {
        {IdRole, "appId"},
        {NameRole, "name"},
        {EntryRole, "entry"},
        {IconRole, "icon"},
        {ColorRole, "color"},
        {BlurbRole, "blurb"},
        {VersionRole, "version"},
        {SizeKbRole, "sizeKb"},
        {DockRole, "dock"},
        {DockOrderRole, "dockOrder"},
        {BuiltinRole, "builtin"}
    };
}

void AppListModel::setItems(QList<AppItem> items)
{
    const int previous = m_items.size();
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
    if (previous != m_items.size())
        emit countChanged();
}

QVariantMap AppListModel::info(const QString &key) const
{
    for (const AppItem &item : m_items) {
        if (item.id != key && item.entry != key)
            continue;
        QVariantMap map;
        map.insert(QStringLiteral("appId"), item.id);
        map.insert(QStringLiteral("name"), item.name);
        map.insert(QStringLiteral("color"), item.color);
        map.insert(QStringLiteral("entry"), item.entry);
        map.insert(QStringLiteral("blurb"), item.blurb);
        map.insert(QStringLiteral("version"), item.version);
        map.insert(QStringLiteral("sizeKb"), item.sizeKb);
        return map;
    }
    return {};
}

QStringList AppListModel::ids() const
{
    QStringList ids;
    for (const AppItem &item : m_items)
        ids.append(item.id);
    return ids;
}

QStringList AppListModel::dockIds() const
{
    QList<AppItem> docked;
    for (const AppItem &item : m_items) {
        if (item.dock)
            docked.append(item);
    }
    std::sort(docked.begin(), docked.end(), [](const AppItem &a, const AppItem &b) {
        return a.dockOrder < b.dockOrder;
    });
    QStringList ids;
    for (const AppItem &item : docked)
        ids.append(item.id);
    return ids;
}

bool AppListModel::contains(const QString &id) const
{
    for (const AppItem &item : m_items) {
        if (item.id == id)
            return true;
    }
    return false;
}

bool AppListModel::isBuiltin(const QString &id) const
{
    for (const AppItem &item : m_items) {
        if (item.id == id)
            return item.builtin;
    }
    return false;
}

QList<AppItem> AppListModel::readDirectory(const QString &directory)
{
    QList<AppItem> items;
    QDir root(directory);
    if (!root.exists())
        return items;

    const auto entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        QFile file(entry.filePath() + QStringLiteral("/manifest.json"));
        if (!file.open(QIODevice::ReadOnly))
            continue;

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject())
            continue;

        const QJsonObject obj = doc.object();
        const QString entryFile = obj.value(QStringLiteral("entry")).toString(QStringLiteral("Main.qml"));
        const QString entryPath = QDir(entry.filePath()).filePath(entryFile);
        if (!QFile::exists(entryPath))
            continue;

        AppItem item;
        item.id = entry.fileName();
        item.name = obj.value(QStringLiteral("name")).toString(item.id);
        item.entry = QUrl::fromLocalFile(entryPath).toString();
        item.color = obj.value(QStringLiteral("color")).toString(QStringLiteral("#3A3A3C"));
        item.blurb = obj.value(QStringLiteral("blurb")).toString();
        item.version = obj.value(QStringLiteral("version")).toString(QStringLiteral("1.0"));
        item.sizeKb = obj.value(QStringLiteral("sizeKb")).toInt(0);
        item.dock = obj.value(QStringLiteral("dock")).toBool(false);
        item.dockOrder = obj.value(QStringLiteral("dockOrder")).toInt(100);
        item.builtin = obj.value(QStringLiteral("builtin")).toBool(false);
        item.order = obj.value(QStringLiteral("order")).toInt(100);

        const QString iconFile = obj.value(QStringLiteral("icon")).toString();
        if (!iconFile.isEmpty()) {
            const QString iconPath = QDir(entry.filePath()).filePath(iconFile);
            if (QFile::exists(iconPath))
                item.icon = QUrl::fromLocalFile(iconPath).toString();
        }

        items.push_back(item);
    }

    std::sort(items.begin(), items.end(), [](const AppItem &a, const AppItem &b) {
        if (a.order != b.order)
            return a.order < b.order;
        return a.name < b.name;
    });
    return items;
}

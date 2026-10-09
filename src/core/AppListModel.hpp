#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

struct AppItem {
    QString id;
    QString name;
    QString entry;
    QString icon;
    QString color;
    bool dock = false;
    int dockOrder = 100;
    bool builtin = false;
    int order = 100;
};

class AppListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        EntryRole,
        IconRole,
        ColorRole,
        DockRole,
        DockOrderRole,
        BuiltinRole
    };

    explicit AppListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();

public:
    void setItems(QList<AppItem> items);
    QStringList ids() const;
    QVariantMap info(const QString &key) const;
    QStringList dockIds() const;
    bool contains(const QString &id) const;
    bool isBuiltin(const QString &id) const;

    static QList<AppItem> readDirectory(const QString &directory);

private:
    QList<AppItem> m_items;
};

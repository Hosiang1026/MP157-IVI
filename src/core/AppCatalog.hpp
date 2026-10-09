#pragma once

#include "AppListModel.hpp"

#include <QAbstractItemModel>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class AppCatalog : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *installed READ installed CONSTANT)
    Q_PROPERTY(QAbstractItemModel *dock READ dock CONSTANT)
    Q_PROPERTY(QAbstractItemModel *userApps READ userApps CONSTANT)
    Q_PROPERTY(QAbstractItemModel *available READ available CONSTANT)
public:
    explicit AppCatalog(QObject *parent = nullptr);

    QAbstractItemModel *installed() const;
    QAbstractItemModel *dock() const;
    QAbstractItemModel *userApps() const;
    QAbstractItemModel *available() const;

    Q_INVOKABLE bool install(const QString &id);
    Q_INVOKABLE bool uninstall(const QString &id);
    Q_INVOKABLE bool addToDock(const QString &id);
    Q_INVOKABLE bool removeFromDock(const QString &id);
    Q_INVOKABLE bool moveDock(const QString &id, int index);
    Q_INVOKABLE bool moveHome(const QString &id, int index);
    Q_INVOKABLE QVariantMap appInfo(const QString &key) const;

private:
    void reload();
    QString detectRoot() const;
    QStringList dockIds() const;
    QStringList homeIds() const;
    void saveDock(const QStringList &ids);
    void applyDock(QList<AppItem> &items) const;
    void applyHome(QList<AppItem> &items) const;

    QString m_root;
    AppListModel *m_installed = nullptr;
    AppListModel *m_available = nullptr;
    QAbstractItemModel *m_dock = nullptr;
    QAbstractItemModel *m_userApps = nullptr;
};

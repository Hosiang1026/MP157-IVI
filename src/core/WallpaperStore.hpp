#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class WallpaperStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString current READ current NOTIFY currentChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
public:
    explicit WallpaperStore(QObject *parent = nullptr);

    QString current() const;
    QVariantList items() const;

    Q_INVOKABLE void select(const QString &path);
    Q_INVOKABLE void upload();

signals:
    void currentChanged();
    void itemsChanged();

private:
    void reload();
    void setCurrent(const QString &filePath);
    bool copyIn(const QString &sourcePath);

    QString m_builtinDir;
    QString m_userDir;
    QString m_current;
    QVariantList m_items;
};

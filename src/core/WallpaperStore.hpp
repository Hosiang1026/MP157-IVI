#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class WallpaperStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString current READ current NOTIFY currentChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(bool darkBackdrop READ darkBackdrop NOTIFY darkBackdropChanged)
public:
    explicit WallpaperStore(QObject *parent = nullptr);

    QString current() const;
    QVariantList items() const;
    bool darkBackdrop() const;

    Q_INVOKABLE void select(const QString &path);
    Q_INVOKABLE QVariantList pickableImages() const;
    Q_INVOKABLE bool importFrom(const QString &path);
    Q_INVOKABLE bool removeCustom(const QString &path);

signals:
    void currentChanged();
    void itemsChanged();
    void darkBackdropChanged();

private:
    void reload();
    void setCurrent(const QString &filePath);
    bool copyIn(const QString &sourcePath);
    void refreshBackdrop();
    QString picturesDir() const;

    QString m_builtinDir;
    QString m_userDir;
    QString m_current;
    QVariantList m_items;
    bool m_darkBackdrop = true;
};

#pragma once

#include <QList>
#include <QObject>
#include <QString>

class AudioFocus : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString owner READ owner NOTIFY ownerChanged)
    Q_PROPERTY(bool ducked READ ducked NOTIFY duckedChanged)
    Q_PROPERTY(int callPriority READ callPriority CONSTANT)
    Q_PROPERTY(int projectionPriority READ projectionPriority CONSTANT)
    Q_PROPERTY(int radioPriority READ radioPriority CONSTANT)
    Q_PROPERTY(int mediaPriority READ mediaPriority CONSTANT)
    Q_PROPERTY(int btPriority READ btPriority CONSTANT)
public:
    explicit AudioFocus(QObject *parent = nullptr);

    QString owner() const;
    bool ducked() const;
    int callPriority() const;
    int projectionPriority() const;
    int radioPriority() const;
    int mediaPriority() const;
    int btPriority() const;

    Q_INVOKABLE void request(const QString &client, int priority);
    Q_INVOKABLE void release(const QString &client);

signals:
    void ownerChanged();
    void duckedChanged();

private:
    struct Hold {
        QString client;
        int priority = 0;
    };

    void refresh();

    QList<Hold> m_holds;
    QString m_owner;
    bool m_ducked = false;
};

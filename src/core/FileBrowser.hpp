#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QVariantList>

class MediaSession;

class FileBrowser : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList roots READ roots NOTIFY rootsChanged)
    Q_PROPERTY(QString rootId READ rootId NOTIFY pathChanged)
    Q_PROPERTY(QString rootName READ rootName NOTIFY pathChanged)
    Q_PROPERTY(QString relativePath READ relativePath NOTIFY pathChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(bool shareRunning READ shareRunning NOTIFY shareChanged)
    Q_PROPERTY(QString shareUrl READ shareUrl NOTIFY shareChanged)
    Q_PROPERTY(QString shareStatus READ shareStatus NOTIFY shareChanged)
    Q_PROPERTY(QString hostIp READ hostIp NOTIFY shareChanged)
    Q_PROPERTY(int sharePort READ sharePort NOTIFY shareChanged)
    Q_PROPERTY(QString lastEvent READ lastEvent NOTIFY lastEventChanged)
    Q_PROPERTY(QString pendingVideo READ pendingVideo NOTIFY pendingVideoChanged)
public:
    explicit FileBrowser(MediaSession *media = nullptr, QObject *parent = nullptr);
    ~FileBrowser() override;

    QVariantList roots() const { return m_roots; }
    QString rootId() const { return m_rootId; }
    QString rootName() const;
    QString relativePath() const { return m_rel; }
    QVariantList entries() const { return m_entries; }
    bool shareRunning() const { return m_shareRunning; }
    QString shareUrl() const;
    QString shareStatus() const { return m_shareStatus; }
    QString hostIp() const { return m_hostIp; }
    int sharePort() const { return int(m_sharePort); }
    QString lastEvent() const { return m_lastEvent; }
    QString pendingVideo() const { return m_pendingVideo; }

    Q_INVOKABLE void openRoot(const QString &id);
    Q_INVOKABLE void enter(const QString &name);
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool removeEntry(const QString &name);
    Q_INVOKABLE bool copyEntry(const QString &name, const QString &destRootId);
    Q_INVOKABLE QVariantList copyTargets(const QString &name) const;
    Q_INVOKABLE bool openEntry(const QString &name);
    Q_INVOKABLE void clearPendingVideo();
    Q_INVOKABLE void startShare();
    Q_INVOKABLE void stopShare();

signals:
    void rootsChanged();
    void pathChanged();
    void entriesChanged();
    void shareChanged();
    void lastEventChanged();
    void pendingVideoChanged();
    void requestOpenApp(const QString &appId);

private:
    struct Client {
        QByteArray buffer;
        qint64 need = -1;
        bool headerDone = false;
    };

    struct Root {
        QString id;
        QString name;
        QString absPath;
        bool removable = false;
    };

    void rebuildRoots();
    void ensureLocalDirs() const;
    QString rootAbs() const;
    QString rootAbsById(const QString &id) const;
    QString inboxAbs() const;
    QString absOf(const QString &rel) const;
    bool underRoot(const QString &abs) const;
    bool underInbox(const QString &abs) const;
    void setShareStatus(const QString &status);
    void setLastEvent(const QString &event);
    void reloadEntries();
    QString primaryIpv4() const;
    void onNewConnection();
    void onReadyRead();
    void handleRequest(QTcpSocket *sock, const QByteArray &raw);
    QByteArray pageHtml() const;
    QByteArray listJson() const;
    bool saveUpload(const QByteArray &body, const QByteArray &boundary, QString *savedName);
    QString safeFileName(const QString &name) const;
    QString uniquePath(const QString &dir, const QString &fileName) const;
    static QString kindOf(const QString &fileName);
    bool isUsbRoot(const QString &id) const;

    MediaSession *m_media = nullptr;
    QList<Root> m_rootList;
    QVariantList m_roots;
    QString m_rootId;
    QString m_rel;
    QVariantList m_entries;
    QTcpServer m_http;
    QHash<QTcpSocket *, Client> m_clients;
    bool m_shareRunning = false;
    QString m_hostIp;
    quint16 m_sharePort = 8787;
    QString m_shareStatus;
    QString m_lastEvent;
    QString m_pendingVideo;
};

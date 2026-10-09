#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class QNetworkReply;

class UpdateService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString currentVersion READ currentVersion NOTIFY changed)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QString notes READ notes NOTIFY changed)
    Q_PROPERTY(qreal progress READ progress NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY changed)
    Q_PROPERTY(QString manifestUrl READ manifestUrl WRITE setManifestUrl NOTIFY changed)
public:
    explicit UpdateService(QObject *parent = nullptr);

    QString status() const;
    QString statusText() const;
    QString currentVersion() const;
    QString latestVersion() const;
    QString notes() const;
    qreal progress() const;
    bool busy() const;
    bool updateAvailable() const;
    QString manifestUrl() const;
    void setManifestUrl(const QString &url);

    Q_INVOKABLE void checkForUpdate();
    Q_INVOKABLE void startUpdate();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void restartNow();

    static QString readInstalledVersion();
    static int compareVersion(const QString &a, const QString &b);

signals:
    void changed();

private:
    void setStatus(const QString &status, const QString &text = QString());
    void fail(const QString &text);
    void abortReply();
    QString installRoot() const;
    QString binaryDir() const;
    QString workDir() const;
    void onManifestFinished();
    void onPackageFinished();
    void beginApply();
    bool extractPackage(const QString &archive, const QString &dest);
    bool launchApplyAndQuit(const QString &staging);
    bool verifySha256(const QString &filePath, const QString &expected) const;

    QNetworkAccessManager m_nam;
    QNetworkReply *m_reply = nullptr;
    QString m_status = QStringLiteral("idle");
    QString m_statusText;
    QString m_currentVersion;
    QString m_latestVersion;
    QString m_notes;
    QString m_packageUrl;
    QString m_sha256;
    qint64 m_size = 0;
    qreal m_progress = 0;
    QString m_manifestUrl;
    QString m_packagePath;
};

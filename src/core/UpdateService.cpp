#include "UpdateService.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {

const QStringList kMergeNames = {
    QStringLiteral("apps"),
    QStringLiteral("feed"),
    QStringLiteral("media"),
    QStringLiteral("maps"),
    QStringLiteral("offline-mfi"),
    QStringLiteral("version.json"),
    QStringLiteral("ivi-shell"),
    QStringLiteral("ivi-shell.exe"),
    QStringLiteral("board-probe"),
    QStringLiteral("run-ivi-shell.sh"),
    QStringLiteral("qt"),
};

bool removePath(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists())
        return true;
    if (info.isDir())
        return QDir(path).removeRecursively();
    return QFile::remove(path);
}

QString findVersionFile()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("version.json")),
        QDir(appDir).filePath(QStringLiteral("../share/mp157-ivi/version.json")),
        QStringLiteral("/opt/ivi/version.json"),
    };
    for (const QString &path : candidates) {
        if (QFile::exists(path))
            return QDir(path).absolutePath();
    }
    return QDir(appDir).filePath(QStringLiteral("version.json"));
}

}

UpdateService::UpdateService(QObject *parent)
    : QObject(parent)
    , m_currentVersion(readInstalledVersion())
{
    m_manifestUrl = QSettings().value(QStringLiteral("update/manifestUrl")).toString();
    if (m_manifestUrl.isEmpty())
        m_manifestUrl = qEnvironmentVariable("IVI_UPDATE_MANIFEST");
    m_statusText = QStringLiteral("当前版本 %1").arg(m_currentVersion);
}

QString UpdateService::status() const { return m_status; }
QString UpdateService::statusText() const { return m_statusText; }
QString UpdateService::currentVersion() const { return m_currentVersion; }
QString UpdateService::latestVersion() const { return m_latestVersion; }
QString UpdateService::notes() const { return m_notes; }
qreal UpdateService::progress() const { return m_progress; }
bool UpdateService::busy() const
{
    return m_status == QLatin1String("checking")
        || m_status == QLatin1String("downloading")
        || m_status == QLatin1String("verifying")
        || m_status == QLatin1String("applying");
}
bool UpdateService::updateAvailable() const
{
    return m_status == QLatin1String("available") || m_status == QLatin1String("ready");
}
QString UpdateService::manifestUrl() const { return m_manifestUrl; }

void UpdateService::setManifestUrl(const QString &url)
{
    if (m_manifestUrl == url)
        return;
    m_manifestUrl = url.trimmed();
    QSettings().setValue(QStringLiteral("update/manifestUrl"), m_manifestUrl);
    emit changed();
}

QString UpdateService::readInstalledVersion()
{
    QFile f(findVersionFile());
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        const QString v = obj.value(QStringLiteral("version")).toString();
        if (!v.isEmpty())
            return v;
    }
    return QStringLiteral("1.0.0.20261009");
}

int UpdateService::compareVersion(const QString &a, const QString &b)
{
    const QStringList la = a.split(QLatin1Char('.'));
    const QStringList lb = b.split(QLatin1Char('.'));
    const int n = qMax(la.size(), lb.size());
    for (int i = 0; i < n; ++i) {
        const qint64 va = i < la.size() ? la[i].toLongLong() : 0;
        const qint64 vb = i < lb.size() ? lb[i].toLongLong() : 0;
        if (va != vb)
            return va < vb ? -1 : 1;
    }
    return 0;
}

void UpdateService::checkForUpdate()
{
    if (busy())
        return;
    if (m_manifestUrl.isEmpty()) {
        fail(QStringLiteral("未配置更新地址"));
        return;
    }
    abortReply();
    m_latestVersion.clear();
    m_notes.clear();
    m_packageUrl.clear();
    m_sha256.clear();
    m_size = 0;
    m_progress = 0;
    setStatus(QStringLiteral("checking"), QStringLiteral("正在检查更新…"));

    QNetworkRequest req{QUrl(m_manifestUrl)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_nam.get(req);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateService::onManifestFinished);
}

void UpdateService::startUpdate()
{
    if (m_status != QLatin1String("available") && m_status != QLatin1String("ready"))
        return;
    if (m_packageUrl.isEmpty()) {
        fail(QStringLiteral("更新包地址无效"));
        return;
    }
    abortReply();
    m_progress = 0;
    const QString dir = workDir();
    QDir().mkpath(dir);
    m_packagePath = QDir(dir).filePath(QStringLiteral("package.tgz"));
    QFile::remove(m_packagePath);
    setStatus(QStringLiteral("downloading"), QStringLiteral("正在下载更新…"));

    QFile *out = new QFile(m_packagePath);
    if (!out->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete out;
        fail(QStringLiteral("无法写入更新包"));
        return;
    }

    QNetworkRequest req{QUrl(m_packageUrl)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_nam.get(req);
    out->setParent(m_reply);
    connect(m_reply, &QNetworkReply::readyRead, this, [this, out] {
        out->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 rec, qint64 total) {
        if (total > 0)
            m_progress = qreal(rec) / qreal(total);
        else if (m_size > 0)
            m_progress = qMin(qreal(0.99), qreal(rec) / qreal(m_size));
        m_statusText = QStringLiteral("下载中 %1%").arg(int(m_progress * 100));
        emit changed();
    });
    connect(m_reply, &QNetworkReply::finished, this, &UpdateService::onPackageFinished);
}

void UpdateService::cancel()
{
    if (!busy())
        return;
    abortReply();
    setStatus(QStringLiteral("idle"), QStringLiteral("当前版本 %1").arg(m_currentVersion));
}

void UpdateService::restartNow()
{
    if (m_status != QLatin1String("ready"))
        return;
    const QString staging = QDir(workDir()).filePath(QStringLiteral("staging"));
    if (!launchApplyAndQuit(staging))
        fail(QStringLiteral("无法启动升级脚本"));
}

void UpdateService::setStatus(const QString &status, const QString &text)
{
    m_status = status;
    if (!text.isNull())
        m_statusText = text;
    emit changed();
}

void UpdateService::fail(const QString &text)
{
    abortReply();
    m_progress = 0;
    setStatus(QStringLiteral("failed"), text);
}

void UpdateService::abortReply()
{
    if (!m_reply)
        return;
    m_reply->disconnect(this);
    m_reply->abort();
    m_reply->deleteLater();
    m_reply = nullptr;
}

QString UpdateService::installRoot() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir,
        QDir(appDir).filePath(QStringLiteral("../share/mp157-ivi")),
        QStringLiteral("/opt/ivi"),
    };
    for (const QString &candidate : candidates) {
        const QString root = QDir(candidate).absolutePath();
        if (QDir(root + QStringLiteral("/apps")).exists())
            return root;
    }
    return QDir(appDir).absolutePath();
}

QString UpdateService::binaryDir() const
{
    return QDir(QCoreApplication::applicationDirPath()).absolutePath();
}

QString UpdateService::workDir() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("mp157-ivi-update"));
}

void UpdateService::onManifestFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (!reply)
        return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        fail(QStringLiteral("检查失败：%1").arg(reply->errorString()));
        return;
    }
    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    m_latestVersion = obj.value(QStringLiteral("version")).toString().trimmed();
    m_packageUrl = obj.value(QStringLiteral("url")).toString().trimmed();
    m_sha256 = obj.value(QStringLiteral("sha256")).toString().trimmed().toLower();
    m_notes = obj.value(QStringLiteral("notes")).toString();
    m_size = static_cast<qint64>(obj.value(QStringLiteral("size")).toDouble());
    if (m_latestVersion.isEmpty() || m_packageUrl.isEmpty()) {
        fail(QStringLiteral("更新描述无效"));
        return;
    }
    if (QUrl(m_packageUrl).isRelative())
        m_packageUrl = QUrl(m_manifestUrl).resolved(QUrl(m_packageUrl)).toString();

    if (compareVersion(m_latestVersion, m_currentVersion) <= 0) {
        setStatus(QStringLiteral("upToDate"), QStringLiteral("已是最新版本 %1").arg(m_currentVersion));
        return;
    }
    setStatus(QStringLiteral("available"),
              QStringLiteral("发现新版本 %1").arg(m_latestVersion));
}

void UpdateService::onPackageFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (!reply)
        return;
    reply->deleteLater();
    if (auto *out = reply->findChild<QFile *>()) {
        if (reply->bytesAvailable() > 0)
            out->write(reply->readAll());
        out->flush();
        out->close();
    }
    if (reply->error() != QNetworkReply::NoError) {
        fail(QStringLiteral("下载失败：%1").arg(reply->errorString()));
        return;
    }

    setStatus(QStringLiteral("verifying"), QStringLiteral("正在校验…"));
    m_progress = 1;
    emit changed();
    if (!m_sha256.isEmpty() && !verifySha256(m_packagePath, m_sha256)) {
        fail(QStringLiteral("校验失败"));
        return;
    }
    beginApply();
}

void UpdateService::beginApply()
{
    setStatus(QStringLiteral("applying"), QStringLiteral("正在安装…"));
    const QString staging = QDir(workDir()).filePath(QStringLiteral("staging"));
    removePath(staging);
    QDir().mkpath(staging);
    if (!extractPackage(m_packagePath, staging)) {
        fail(QStringLiteral("解压失败"));
        return;
    }
    if (!launchApplyAndQuit(staging))
        fail(QStringLiteral("无法启动升级脚本"));
}

bool UpdateService::extractPackage(const QString &archive, const QString &dest)
{
    QProcess proc;
    proc.setProgram(QStringLiteral("tar"));
    proc.setArguments({QStringLiteral("-xzf"), archive, QStringLiteral("-C"), dest});
    proc.start();
    if (!proc.waitForStarted(5000))
        return false;
    return proc.waitForFinished(600000) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

bool UpdateService::launchApplyAndQuit(const QString &staging)
{
    const QString root = installRoot();
    const QString bin = binaryDir();
    const QString scriptDir = workDir();
    QDir().mkpath(scriptDir);

#ifdef Q_OS_WIN
    const QString script = QDir(scriptDir).filePath(QStringLiteral("apply-update.cmd"));
    const QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    QString body;
    body += QStringLiteral("@echo off\r\n");
    body += QStringLiteral("ping 127.0.0.1 -n 3 >nul\r\n");
    for (const QString &name : kMergeNames) {
        const QString src = QDir(staging).filePath(name);
        if (!QFileInfo::exists(src))
            continue;
        QString dst;
        if (name == QLatin1String("ivi-shell") || name == QLatin1String("ivi-shell.exe")
            || name == QLatin1String("board-probe"))
            dst = QDir(bin).filePath(name);
        else
            dst = QDir(root).filePath(name);
        const QString srcNative = QDir::toNativeSeparators(src);
        const QString dstNative = QDir::toNativeSeparators(dst);
        if (QFileInfo(src).isDir()) {
            body += QStringLiteral("if exist \"%1\" rmdir /s /q \"%1\"\r\n").arg(dstNative);
            body += QStringLiteral("xcopy /E /I /Y \"%1\" \"%2\\\" >nul\r\n").arg(srcNative, dstNative);
        } else {
            body += QStringLiteral("if exist \"%1\" del /f /q \"%1\"\r\n").arg(dstNative);
            body += QStringLiteral("mkdir \"%1\" >nul 2>nul\r\n")
                        .arg(QDir::toNativeSeparators(QFileInfo(dst).absolutePath()));
            body += QStringLiteral("copy /y \"%1\" \"%2\" >nul\r\n").arg(srcNative, dstNative);
        }
    }
    body += QStringLiteral("start \"\" \"%1\"\r\n")
                .arg(QDir::toNativeSeparators(QDir(bin).filePath(exeName)));
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    f.write(body.toLocal8Bit());
    f.close();
    if (!QProcess::startDetached(QStringLiteral("cmd.exe"),
                                 {QStringLiteral("/c"), script},
                                 scriptDir))
        return false;
#else
    const QString script = QDir(scriptDir).filePath(QStringLiteral("apply-update.sh"));
    QString body;
    body += QStringLiteral("#!/bin/sh\n");
    body += QStringLiteral("sleep 1\n");
    body += QStringLiteral("STAGING='%1'\n").arg(staging);
    body += QStringLiteral("ROOT='%1'\n").arg(root);
    body += QStringLiteral("BIN='%1'\n").arg(bin);
    for (const QString &name : kMergeNames) {
        body += QStringLiteral("if [ -e \"$STAGING/%1\" ]; then\n").arg(name);
        if (name == QLatin1String("ivi-shell") || name == QLatin1String("board-probe"))
            body += QStringLiteral("  DST=\"$BIN/%1\"\n").arg(name);
        else
            body += QStringLiteral("  DST=\"$ROOT/%1\"\n").arg(name);
        body += QStringLiteral("  rm -rf \"$DST\"\n");
        body += QStringLiteral("  cp -a \"$STAGING/%1\" \"$DST\"\n").arg(name);
        body += QStringLiteral("fi\n");
    }
    body += QStringLiteral("if command -v systemctl >/dev/null 2>&1 && systemctl is-enabled ivi-shell >/dev/null 2>&1; then\n");
    body += QStringLiteral("  systemctl restart ivi-shell || true\n");
    body += QStringLiteral("elif [ -x \"$ROOT/run-ivi-shell.sh\" ]; then\n");
    body += QStringLiteral("  \"$ROOT/run-ivi-shell.sh\" &\n");
    body += QStringLiteral("elif [ -x \"$BIN/ivi-shell\" ]; then\n");
    body += QStringLiteral("  \"$BIN/ivi-shell\" &\n");
    body += QStringLiteral("fi\n");
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    f.write(body.toUtf8());
    f.close();
    QFile::setPermissions(script,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                              | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                              | QFileDevice::ReadOther | QFileDevice::ExeOther);
    if (!QProcess::startDetached(QStringLiteral("/bin/sh"), {script}, scriptDir))
        return false;
#endif

    setStatus(QStringLiteral("ready"), QStringLiteral("即将重启以完成升级"));
    QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
    return true;
}

bool UpdateService::verifySha256(const QString &filePath, const QString &expected) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f))
        return false;
    return QString::fromLatin1(hash.result().toHex()) == expected;
}

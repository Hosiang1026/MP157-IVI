#pragma once

#include <QByteArray>
#include <QString>

class LocalMfiAuth {
public:
    static QString defaultDirectory();

    bool load(const QString &directory);
    bool isReady() const;
    QString errorString() const;
    int protocolMajor() const;
    QByteArray certificate() const;
    QByteArray signChallenge(const QByteArray &challenge) const;

private:
    bool verifyKeyMatchesCert() const;

    QByteArray m_pk8;
    QByteArray m_certificate;
    QString m_error;
    bool m_ready = false;
};

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

class AudioFocus;

class CallSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString number READ number NOTIFY infoChanged)
    Q_PROPERTY(QString contactName READ contactName NOTIFY infoChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY mutedChanged)
    Q_PROPERTY(bool speaker READ speaker NOTIFY speakerChanged)
    Q_PROPERTY(int elapsed READ elapsed NOTIFY elapsedChanged)
    Q_PROPERTY(QVariantList contacts READ contacts CONSTANT)
    Q_PROPERTY(QVariantList recents READ recents NOTIFY recentsChanged)
public:
    CallSession(AudioFocus *audio, QObject *parent = nullptr);

    bool active() const;
    QString number() const;
    QString contactName() const;
    bool muted() const;
    bool speaker() const;
    int elapsed() const;
    QVariantList contacts() const;
    QVariantList recents() const;

    Q_INVOKABLE void dial();
    Q_INVOKABLE void dialNumber(const QString &number);
    Q_INVOKABLE void hangup();
    Q_INVOKABLE void toggleMuted();
    Q_INVOKABLE void toggleSpeaker();

signals:
    void activeChanged();
    void infoChanged();
    void mutedChanged();
    void speakerChanged();
    void elapsedChanged();
    void recentsChanged();

private:
    QString lookup(const QString &number) const;

    AudioFocus *m_audio = nullptr;
    QVariantList m_contacts;
    QVariantList m_recents;
    QString m_number;
    QString m_name;
    bool m_active = false;
    bool m_muted = false;
    bool m_speaker = false;
    int m_elapsed = 0;
    QTimer m_timer;
};

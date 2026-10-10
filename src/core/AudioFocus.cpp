#include "AudioFocus.hpp"

AudioFocus::AudioFocus(QObject *parent)
    : QObject(parent)
{
}

QString AudioFocus::owner() const
{
    return m_owner;
}

bool AudioFocus::ducked() const
{
    return m_ducked;
}

int AudioFocus::callPriority() const
{
    return 10;
}

int AudioFocus::projectionPriority() const
{
    return 8;
}

int AudioFocus::radioPriority() const
{
    return 3;
}

int AudioFocus::mediaPriority() const
{
    return 2;
}

int AudioFocus::btPriority() const
{
    return 1;
}

void AudioFocus::request(const QString &client, int priority)
{
    if (client.isEmpty())
        return;

    for (int i = m_holds.size() - 1; i >= 0; --i) {
        if (m_holds.at(i).client == client)
            m_holds.removeAt(i);
    }
    m_holds.push_back({client, priority});
    refresh();
}

void AudioFocus::release(const QString &client)
{
    bool removed = false;
    for (int i = m_holds.size() - 1; i >= 0; --i) {
        if (m_holds.at(i).client == client) {
            m_holds.removeAt(i);
            removed = true;
        }
    }
    if (removed)
        refresh();
}

void AudioFocus::refresh()
{
    QString owner;
    int best = -1;
    for (const Hold &hold : m_holds) {
        if (hold.priority >= best) {
            best = hold.priority;
            owner = hold.client;
        }
    }

    bool hasLower = false;
    for (const Hold &hold : m_holds) {
        if (hold.priority < best)
            hasLower = true;
    }

    if (owner != m_owner) {
        m_owner = owner;
        emit ownerChanged();
    }
    if (hasLower != m_ducked) {
        m_ducked = hasLower;
        emit duckedChanged();
    }
}

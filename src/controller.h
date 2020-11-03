/**
 * SPDX-FileCopyrightText: Black Hat <bhat@encom.eu.org>
 *
 * SPDX-LicenseIdentifier: GPL-3.0-only
 */
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QApplication>
#include <QMediaPlayer>
#include <QMenu>
#include <QNetworkConfigurationManager>
#include <QObject>
#include <QSystemTrayIcon>

#include "connection.h"
#include "csapi/list_public_rooms.h"
#include "room.h"
#include "settings.h"
#include "user.h"

using namespace Quotient;

class Controller : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int accountCount READ accountCount NOTIFY connectionAdded NOTIFY connectionDropped)
    Q_PROPERTY(bool quitOnLastWindowClosed READ quitOnLastWindowClosed WRITE setQuitOnLastWindowClosed NOTIFY quitOnLastWindowClosedChanged)
    Q_PROPERTY(Connection *connection READ connection WRITE setConnection NOTIFY connectionChanged)
    Q_PROPERTY(bool isOnline READ isOnline NOTIFY isOnlineChanged)
    Q_PROPERTY(bool busy READ busy WRITE setBusy NOTIFY busyChanged)

public:
    explicit Controller(QObject *parent = nullptr);
    ~Controller();

    Q_INVOKABLE void loginWithCredentials(QString, QString, QString, QString);
    Q_INVOKABLE void loginWithAccessToken(QString, QString, QString, QString);

    QVector<Connection *> connections() const
    {
        return m_connections;
    }

    // All the non-Q_INVOKABLE functions.
    void addConnection(Connection *c);
    void dropConnection(Connection *c);

    // All the Q_PROPERTYs.
    int accountCount()
    {
        return m_connections.count();
    }

    bool quitOnLastWindowClosed() const
    {
        return QApplication::quitOnLastWindowClosed();
    }
    void setQuitOnLastWindowClosed(bool value)
    {
        if (quitOnLastWindowClosed() != value) {
            QApplication::setQuitOnLastWindowClosed(value);

            Q_EMIT quitOnLastWindowClosedChanged();
        }
    }

    bool isOnline() const
    {
        return m_ncm.isOnline();
    }

    bool busy() const
    {
        return m_busy;
    }
    void setBusy(bool busy)
    {
        if (m_busy == busy) {
            return;
        }
        m_busy = busy;
        Q_EMIT busyChanged();
    }

    Connection *connection() const
    {
        if (m_connection.isNull())
            return nullptr;

        return m_connection;
    }

    void setConnection(Connection *conn)
    {
        if (conn == m_connection)
            return;
        m_connection = conn;
        Q_EMIT connectionChanged();
    }

private:
    QVector<Connection *> m_connections;
    QPointer<Connection> m_connection;
    QNetworkConfigurationManager m_ncm;
    bool m_busy = false;

    QByteArray loadAccessTokenFromFile(const AccountSettings &account);
    QByteArray loadAccessTokenFromKeyChain(const AccountSettings &account);

    bool saveAccessTokenToFile(const AccountSettings &account, const QByteArray &accessToken);
    bool saveAccessTokenToKeyChain(const AccountSettings &account, const QByteArray &accessToken);
    void loadSettings();
    void saveSettings() const;

private slots:
    void invokeLogin();

signals:
    void busyChanged();
    void errorOccured(QString error, QString detail);
    void syncDone();
    void connectionAdded(Connection *conn);
    void connectionDropped(Connection *conn);
    void initiated();
    void notificationClicked(const QString roomId, const QString eventId);
    void quitOnLastWindowClosedChanged();
    void unreadCountChanged();
    void connectionChanged();
    void isOnlineChanged();

public slots:
    void logout(Connection *conn);
    void joinRoom(Connection *c, const QString &alias);
    void createRoom(Connection *c, const QString &name, const QString &topic);
    void createDirectChat(Connection *c, const QString &userID);
    void playAudio(QUrl localFile);
    void changeAvatar(Connection *conn, QUrl localFile);
    void markAllMessagesAsRead(Connection *conn);
};

#endif // CONTROLLER_H

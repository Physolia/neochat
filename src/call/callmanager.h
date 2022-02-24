// SPDX-FileCopyrightText: 2020-2021 Nheko Authors
// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "neochatroom.h"
#include "neochatuser.h"
#include <QObject>
#include <QString>
#include <events/roomevent.h>

#include "callsession.h"

#include "events/callanswerevent.h"
#include "events/callcandidatesevent.h"
#include "events/callhangupevent.h"
#include "events/callinviteevent.h"

#include <qcoro/task.h>

using namespace Quotient;
class CallManager : public QObject
{
    Q_OBJECT

public:
    enum GlobalState {
        IDLE,
        INCOMING,
        OUTGOING,
        ACTIVE,
    };
    Q_ENUM(GlobalState);

    Q_PROPERTY(GlobalState globalState READ globalState NOTIFY globalStateChanged);
    Q_PROPERTY(CallSession::State state READ state NOTIFY stateChanged);
    Q_PROPERTY(NeoChatUser *remoteUser READ remoteUser NOTIFY remoteUserChanged);
    Q_PROPERTY(QString callId READ callId NOTIFY callIdChanged);
    Q_PROPERTY(NeoChatRoom *room READ room NOTIFY roomChanged);
    Q_PROPERTY(int lifetime READ lifetime NOTIFY lifetimeChanged); // TODO integrate with 'hasInvite'
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged);
    Q_PROPERTY(QQuickItem *item MEMBER m_item); // TODO allow for different devices for each session

    static CallManager &instance()
    {
        static CallManager _instance;
        return _instance;
    }

    QString callId() const;
    CallSession::State state() const;
    NeoChatUser *remoteUser() const;
    NeoChatRoom *room() const;
    bool hasInvite() const;
    bool isInviting() const;

    int lifetime() const;

    bool muted() const;
    void setMuted(bool muted);

    CallManager::GlobalState globalState() const;

    void handleCallEvent(NeoChatRoom *room, const RoomEvent *event);

    Q_INVOKABLE void startCall(NeoChatRoom *room, bool camera);
    Q_INVOKABLE void acceptCall();
    Q_INVOKABLE void hangupCall();
    Q_INVOKABLE void ignoreCall();

    QCoro::Task<void> updateTurnServers();

    QQuickItem *m_item = nullptr;

Q_SIGNALS:
    void currentCallIdChanged();
    void incomingCall(NeoChatUser *user, NeoChatRoom *room, int timeout, const QString &callId);
    void callEnded();
    void remoteUserChanged();
    void callIdChanged();
    void roomChanged();
    void hasInviteChanged();
    void stateChanged();
    void lifetimeChanged();
    void isInvitingChanged();
    void mutedChanged();
    void globalStateChanged();

private:
    CallManager();
    QString m_callId;
    QVector<Candidate> m_incomingCandidates;
    QString m_incomingSDP;

    void checkPlugins(bool isVideo);

    QStringList m_cachedTurnUris;
    QDateTime m_cachedTurnUrisValidUntil = QDateTime::fromSecsSinceEpoch(0);

    NeoChatUser *m_remoteUser = nullptr;
    NeoChatRoom *m_room = nullptr;
    int m_lifetime = 0;

    bool m_hasInvite = false;
    bool m_isInviting = false;
    GlobalState m_globalState;

    void handleInvite(NeoChatRoom *room, const CallInviteEvent *event);
    void handleHangup(NeoChatRoom *room, const CallHangupEvent *event);
    void handleCandidates(NeoChatRoom *room, const CallCandidatesEvent *event);
    void handleAnswer(NeoChatRoom *room, const CallAnswerEvent *event);

    QString generateCallId();
    bool init();

    bool m_initialised = false;
    CallSession *m_session = nullptr;

    void setLifetime(int lifetime);
    void setRoom(NeoChatRoom *room);
    void setRemoteUser(NeoChatUser *user);
    void setCallId(const QString &callid);
    void setGlobalState(GlobalState state);

    NeoChatUser *otherUser(NeoChatRoom *room);
    QJsonArray candidatesToJson(const QVector<Candidate> &candidates) const;
};

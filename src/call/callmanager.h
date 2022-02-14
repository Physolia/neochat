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

using namespace Quotient;
class CallManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool hasInvite READ hasInvite NOTIFY hasInviteChanged);
    Q_PROPERTY(bool isInviting READ isInviting NOTIFY isInvitingChanged);
    Q_PROPERTY(CallSession::State state READ state NOTIFY stateChanged);
    Q_PROPERTY(NeoChatUser *remoteUser READ remoteUser NOTIFY remoteUserChanged);
    Q_PROPERTY(QString callId READ callId NOTIFY callIdChanged);
    Q_PROPERTY(NeoChatRoom *room READ room NOTIFY roomChanged);
    Q_PROPERTY(int lifetime READ lifetime NOTIFY lifetimeChanged); // TODO integrate with 'hasInvite'
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged);
    Q_PROPERTY(QQuickItem *item MEMBER m_item); // TODO allow for different devices for each session

public:
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

    void handleCallEvent(NeoChatRoom *room, const RoomEvent *event);

    Q_INVOKABLE void startCall(NeoChatRoom *room, bool camera);
    Q_INVOKABLE void acceptCall();
    Q_INVOKABLE void hangupCall();
    Q_INVOKABLE void ignoreCall();

    void updateTurnServers();

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

private:
    CallManager();
    QString m_callId;
    QVector<Candidate> m_incomingCandidates;
    QString m_incomingSDP;

    bool m_isVideo = false;

    void checkPlugins(bool isVideo);

    QStringList m_turnUris;

    NeoChatUser *m_remoteUser = nullptr;
    NeoChatRoom *m_room = nullptr;
    int m_lifetime = 0;

    bool m_hasInvite = false;
    bool m_isInviting = false;

    void handleInvite(NeoChatRoom *room, const CallInviteEvent *event);
    void handleHangup(NeoChatRoom *room, const CallHangupEvent *event);
    void handleCandidates(NeoChatRoom *room, const CallCandidatesEvent *event);
    void handleAnswer(NeoChatRoom *room, const CallAnswerEvent *event);

    void generateCallId();
    bool init();

    bool m_initialised = false;
    CallSession *m_session = nullptr;
};

/**
 * SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
 * SPDX-FileCopyrightText: 2020-2021 Nheko Authors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "callmanager.h"

#include "controller.h"

CallManager::CallManager()
{
    connect(&CallSession::instance(), &CallSession::stateChanged, this, &CallManager::stateChanged);
    connect(&Controller::instance(), &Controller::activeConnectionChanged, this, [=](){
        Controller::instance().activeConnection()->getTurnServers();
        connect(Controller::instance().activeConnection(), &Connection::turnServersChanged, [=](const QJsonObject &servers){
            auto ttl = servers["ttl"].toInt();
            QTimer::singleShot(ttl * 800, this, [=](){
                Controller::instance().activeConnection()->getTurnServers();
            });

            auto password = servers["password"].toString();
            auto username = servers["username"].toString();
            auto uris = servers["uris"].toArray();

            m_turnUris.clear();
            for(const auto &u : uris) {
                QString uri = u.toString();
                auto c = uri.indexOf(':');
                if(c == -1) {
                    qDebug() << "Invalid TURN URI:" << uri;
                    continue;
                }
                QString scheme = uri.left(c);
                if(scheme != "turn" && scheme != "turns") {
                    qDebug() << "Invalid TURN scheme:" << scheme;
                    continue;
                }
                m_turnUris += scheme + QStringLiteral("://") + QUrl::toPercentEncoding(username) + QStringLiteral(":") + QUrl::toPercentEncoding(password) + QStringLiteral("@") + uri.mid(c+1);
            }
        });
    });
}

QString CallManager::callId() const
{
    return m_callId;
}

void CallManager::handleCallEvent(NeoChatRoom* room, const Quotient::RoomEvent* event)
{
    if(const auto &inviteEvent = eventCast<const CallInviteEvent>(event)) {
        handleInvite(room, inviteEvent);
    } else if(const auto &hangupEvent = eventCast<const CallHangupEvent>(event)) {
        handleHangup(room, hangupEvent);
    } else if(const auto &candidatesEvent = eventCast<const CallCandidatesEvent>(event)) {
        handleCandidates(room, candidatesEvent);
    } else if(const auto &answerEvent = eventCast<const CallAnswerEvent>(event)) {
        handleAnswer(room, answerEvent);
    } else {
        Q_ASSERT(false);
    }
}

void CallManager::handleAnswer(NeoChatRoom* room, const Quotient::CallAnswerEvent* event)
{
    if(event->senderId() == room->localUser()->id() && event->callId() == m_callId) {
        if(state() == CallSession::DISCONNECTED) {
            //TODO: Show the user that the call was answered on another device
            //TODO: Stop ringing
        }
        return;
    }
    if(state() != CallSession::DISCONNECTED && event->callId() == m_callId) {
        CallSession::instance().acceptAnswer(event->sdp());
    }
    m_isInviting = false;
    Q_EMIT isInvitingChanged();
}

void CallManager::handleCandidates(NeoChatRoom* room, const Quotient::CallCandidatesEvent* event)
{
    if(event->senderId() == room->localUser()->id()) {
        return;
    }
    if(event->callId() != m_callId) {
        return;
    }

    if(state() != CallSession::DISCONNECTED) {
        QVector<Candidate> candidates;
        for(const auto &c : event->candidates()) {
            candidates += Candidate{c.toObject()["candidate"].toString(), c.toObject()["sdpMLineIndex"].toInt(), c.toObject()["sdpMid"].toString()};
        }
        CallSession::instance().acceptICECandidates(candidates);
    } else {
        m_incomingCandidates.clear();
        for(const auto &c : event->candidates()) {
            m_incomingCandidates += Candidate{c.toObject()["candidate"].toString(), c.toObject()["sdpMLineIndex"].toInt(), c.toObject()["sdpMid"].toString()};
        }
    }
}

void CallManager::handleInvite(NeoChatRoom* room, const Quotient::CallInviteEvent* event)
{
    if(state() != CallSession::DISCONNECTED) {
        return;
    }
    m_incomingSDP = event->sdp();
    m_remoteUser = dynamic_cast<NeoChatUser *>(room->user(event->senderId()));
    Q_EMIT remoteUserChanged();
    m_room = room;
    Q_EMIT roomChanged();
    m_callId = event->callId();
    Q_EMIT callIdChanged();
    Q_EMIT incomingCall(static_cast<NeoChatUser *>(room->user(event->senderId())), room, event->lifetime(), event->callId());
    //TODO: Start ringing;
    m_hasInvite = true;
    Q_EMIT hasInviteChanged();
    m_lifetime = event->lifetime();
    Q_EMIT lifetimeChanged();
    QTimer::singleShot(event->lifetime(), this, [=](){
        m_hasInvite = false;
        Q_EMIT hasInviteChanged();
    });
}

void CallManager::handleHangup(NeoChatRoom* room, const Quotient::CallHangupEvent* event)
{
    if(event->callId() != m_callId) {
        return;
    }
    Q_EMIT callEnded();
}

void CallManager::acceptCall()
{
    if(!hasInvite()) {
        return;
    }
    checkPlugins(m_isVideo);

    CallSession::instance().setTurnServers(m_turnUris);
    CallSession::instance().acceptOffer(m_incomingSDP);
    CallSession::instance().acceptICECandidates(m_incomingCandidates);
    m_incomingCandidates.clear();
    connect(&CallSession::instance(), &CallSession::answerCreated, this, [=](const QString &sdp, const QVector<Candidate> &candidates){
        m_room->answerCall(m_callId, sdp);
        QJsonArray cands;
        for(const auto &candidate : candidates) {
            QJsonObject c;
            c["candidate"] = candidate.candidate;
            c["sdpMid"] = candidate.sdpMid;
            c["sdpMLineIndex"] = candidate.sdpMLineIndex;
            cands += c;
        }
        m_room->sendCallCandidates(m_callId, cands);
    });
    m_hasInvite = false;
    Q_EMIT hasInviteChanged();
}

void CallManager::hangupCall()
{
    CallSession::instance().end();
    m_room->hangupCall(m_callId);
    m_isInviting = false;
    m_hasInvite = false;
    Q_EMIT isInvitingChanged();
    Q_EMIT hasInviteChanged();
}

void CallManager::checkPlugins(bool isVideo)
{
    CallSession::instance().havePlugins(isVideo);
}

NeoChatUser *CallManager::remoteUser() const
{
    return m_remoteUser;
}

NeoChatRoom *CallManager::room() const
{
    return m_room;
}

bool CallManager::hasInvite() const
{
    return m_hasInvite;
}

CallSession::State CallManager::state() const
{
    return CallSession::instance().state();
}

int CallManager::lifetime() const
{
    return m_lifetime;
}

void CallManager::ignoreCall()
{
    m_lifetime = 0;
    Q_EMIT lifetimeChanged();
    m_callId = QString();
    Q_EMIT callIdChanged();
    m_hasInvite = false;
    Q_EMIT hasInviteChanged();
    m_room = nullptr;
    Q_EMIT roomChanged();
    m_remoteUser = nullptr;
    Q_EMIT remoteUserChanged();
}

void CallManager::startCall(NeoChatRoom* room)
{

    if(state() != CallSession::DISCONNECTED) {
        return;
    }
    if(room->users().size() != 2) {
        qDebug() << "Room size != 2; aborting mission.";
        return;
    }
    checkPlugins(false); //TODO: video

    m_lifetime = 60000;
    Q_EMIT lifetimeChanged();

    m_room = room;
    m_remoteUser = dynamic_cast<NeoChatUser *>(room->users()[0]->id() == room->localUser()->id() ? room->users()[1] : room->users()[0]);
    Q_EMIT roomChanged();
    Q_EMIT remoteUserChanged();

    CallSession::instance().setTurnServers(m_turnUris);
    generateCallId();

    CallSession::instance().createOffer(CallSession::VOICE);
    m_isInviting = true;
    Q_EMIT isInvitingChanged();

    connect(&CallSession::instance(), &CallSession::offerCreated, this, [=](const QString &sdp, const QVector<Candidate> &candidates){
        m_room->inviteCall(m_callId, 60000, sdp);
        QJsonArray cands;
        for(const auto &candidate : candidates) {
            QJsonObject c;
            c["candidate"] = candidate.candidate;
            c["sdpMid"] = candidate.sdpMid;
            c["sdpMLineIndex"] = candidate.sdpMLineIndex;
            cands += c;
        }
        m_room->sendCallCandidates(m_callId, cands);
        qDebug() << "CallManager: call candidates sent.";
    });
}

void CallManager::generateCallId()
{
    m_callId = QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz");
    Q_EMIT callIdChanged();
}

bool CallManager::isInviting() const
{
    return m_isInviting;
}


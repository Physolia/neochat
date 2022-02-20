// SPDX-FileCopyrightText: 2020-2021 Nheko Authors
// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "callmanager.h"

#include "controller.h"

#include <gst/gst.h>

#include "voiplogging.h"
#include <QDateTime>

CallManager::CallManager()
    : m_session(new CallSession()) // TODO make sure we don't leak these
{
    init();
    connect(m_session, &CallSession::stateChanged, this, [this] {
        Q_EMIT stateChanged();
        if (state() == CallSession::ICEFAILED) {
            Q_EMIT callEnded();
        }
    });
    connect(&Controller::instance(), &Controller::activeConnectionChanged, this, &CallManager::updateTurnServers);
}

void CallManager::updateTurnServers()
{
    disconnect(nullptr, &Connection::turnServersChanged, this, nullptr);
    Controller::instance().activeConnection()->getTurnServers();
    connect(Controller::instance().activeConnection(), &Connection::turnServersChanged, [=](const QJsonObject &servers) {
        auto ttl = servers["ttl"].toInt();
        qDebug() << "TTL" << ttl;
        //         QTimer::singleShot(ttl * 800, this, [=]() {
        //             Controller::instance().activeConnection()->getTurnServers();
        //         });

        auto password = servers["password"].toString();
        auto username = servers["username"].toString();
        auto uris = servers["uris"].toArray();

        m_turnUris.clear();
        for (const auto &u : uris) {
            QString uri = u.toString();
            auto c = uri.indexOf(':');
            if (c == -1) {
                qDebug() << "Invalid TURN URI:" << uri;
                continue;
            }
            QString scheme = uri.left(c);
            if (scheme != "turn" && scheme != "turns") {
                qDebug() << "Invalid TURN scheme:" << scheme;
                continue;
            }
            m_turnUris += scheme + QStringLiteral("://") + QUrl::toPercentEncoding(username) + QStringLiteral(":") + QUrl::toPercentEncoding(password)
                + QStringLiteral("@") + uri.mid(c + 1);
        }
    });
}

QString CallManager::callId() const
{
    return m_callId;
}

void CallManager::handleCallEvent(NeoChatRoom *room, const Quotient::RoomEvent *event)
{
    if (const auto &inviteEvent = eventCast<const CallInviteEvent>(event)) {
        handleInvite(room, inviteEvent);
    } else if (const auto &hangupEvent = eventCast<const CallHangupEvent>(event)) {
        handleHangup(room, hangupEvent);
    } else if (const auto &candidatesEvent = eventCast<const CallCandidatesEvent>(event)) {
        handleCandidates(room, candidatesEvent);
    } else if (const auto &answerEvent = eventCast<const CallAnswerEvent>(event)) {
        handleAnswer(room, answerEvent);
    } else {
        Q_ASSERT(false);
    }
}

void CallManager::handleAnswer(NeoChatRoom *room, const Quotient::CallAnswerEvent *event)
{
    if (event->senderId() != room->localUser()->id()) {
        return;
    }
    if (event->callId() != m_callId) {
        return;
    }
    if (event->senderId() == room->localUser()->id() && event->callId() == m_callId) {
        if (state() == CallSession::DISCONNECTED) {
            // TODO: Show the user that the call was answered on another device
            // TODO: Stop ringing
        }
        return;
    }

    if (state() != CallSession::DISCONNECTED && event->callId() == m_callId) {
        m_session->acceptAnswer(event->sdp());
    }
    m_isInviting = false;
    Q_EMIT isInvitingChanged();
}

void CallManager::handleCandidates(NeoChatRoom *room, const Quotient::CallCandidatesEvent *event)
{
    if (event->senderId() == room->localUser()->id()) {
        return;
    }
    if (!m_callId.isEmpty() && event->callId() != m_callId) { // temp: don't accept candidates if there is a callid
        qDebug() << "candidates not for this call";
        return;
    }
    // m_incomingCandidates.clear();
    if (state() != CallSession::DISCONNECTED) {
        qDebug() << "Not disconnected";
        QVector<Candidate> candidates;
        for (const auto &c : event->candidates()) {
            candidates += Candidate{c.toObject()["candidate"].toString(), c.toObject()["sdpMLineIndex"].toInt(), c.toObject()["sdpMid"].toString()};
        }
        m_session->acceptICECandidates(candidates);
    } else {
        qDebug() << "Disconnected, saving for later";
        qWarning() << event->candidates();
        m_incomingCandidates.clear();
        for (const auto &c : event->candidates()) {
            m_incomingCandidates += Candidate{c.toObject()["candidate"].toString(), c.toObject()["sdpMLineIndex"].toInt(), c.toObject()["sdpMid"].toString()};
        }
    }
}

void CallManager::handleInvite(NeoChatRoom *room, const Quotient::CallInviteEvent *event)
{
    if (event->originTimestamp() < QDateTime::currentDateTime().addSecs(-60)) {
        return;
    }
    if (event->senderId() == room->localUser()->id()) {
        qDebug() << "Sent by this user";
        return;
    }
    if (m_callId == event->callId()) {
        return;
    }
    if (state() != CallSession::DISCONNECTED) {
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
    // TODO: Start ringing;
    m_hasInvite = true;
    Q_EMIT hasInviteChanged();
    m_lifetime = event->lifetime();
    Q_EMIT lifetimeChanged();
    QTimer::singleShot(event->lifetime(), this, [=]() {
        m_hasInvite = false;
        Q_EMIT hasInviteChanged();
    });
    // acceptCall(); //TODO remove
}

void CallManager::handleHangup(NeoChatRoom *room, const Quotient::CallHangupEvent *event)
{
    if (event->senderId() == room->localUser()->id()) {
        qDebug() << "Sent by this user";
        return;
    }
    if (event->callId() != m_callId) {
        return;
    }
    m_session->end();
    Q_EMIT callEnded();
}

void CallManager::acceptCall()
{
    if (!hasInvite()) {
        return;
    }
    checkPlugins(m_isVideo);

    m_session->setSendVideo(true); // TODO change
    m_session->setTurnServers(m_turnUris);
    m_session->acceptOffer(m_incomingSDP);
    m_session->acceptICECandidates(m_incomingCandidates);
    m_incomingCandidates.clear();
    connectSingleShot(m_session, &CallSession::answerCreated, this, [=](const QString &sdp, const QVector<Candidate> &candidates) {
        qDebug() << "Sending call candidates";
        qWarning() << sdp;
        m_room->answerCall(m_callId, sdp);
        QJsonArray cands;
        for (const auto &candidate : candidates) {
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
    m_session->end();
    m_room->hangupCall(m_callId);
    m_isInviting = false;
    m_hasInvite = false;
    Q_EMIT isInvitingChanged();
    Q_EMIT hasInviteChanged();
}

void CallManager::checkPlugins(bool isVideo)
{
    m_session->havePlugins(isVideo);
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
    return m_session->state();
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

void CallManager::startCall(NeoChatRoom *room, bool camera)
{
    if (state() != CallSession::DISCONNECTED) {
        return;
    }
    if (room->users().size() != 2) {
        qCWarning(voip) << "This room doesn't have exactly two members; aborting mission.";
        return;
    }
    checkPlugins(false); // TODO: video

    m_lifetime = 60000;
    Q_EMIT lifetimeChanged();

    m_room = room;
    m_remoteUser = dynamic_cast<NeoChatUser *>(room->users()[0]->id() == room->localUser()->id() ? room->users()[1] : room->users()[0]);
    Q_EMIT roomChanged();
    Q_EMIT remoteUserChanged();

    m_session->setTurnServers(m_turnUris);
    generateCallId();
    m_session->setSendVideo(camera);

    m_session->startCall();
    m_isInviting = true;
    Q_EMIT isInvitingChanged();

    connectSingleShot(m_session, &CallSession::offerCreated, this, [this](const QString &sdp, const QVector<Candidate> &candidates) {
        m_room->inviteCall(m_callId, 60000, sdp);
        QJsonArray cands;
        for (const auto &candidate : candidates) {
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

void CallManager::setMuted(bool muted)
{
    m_session->setMuted(muted);
    Q_EMIT mutedChanged();
}

bool CallManager::muted() const
{
    return m_session->muted();
}

bool CallManager::init()
{
    if (m_initialised) {
        return true;
    }

    GError *error = nullptr;
    if (!gst_init_check(nullptr, nullptr, &error)) {
        QString strError("Failed to initialise GStreamer: ");
        if (error) {
            strError += error->message;
            g_error_free(error);
        }
        qCCritical(voip) << strError;
        return false;
    }

    m_initialised = true;
    gchar *version = gst_version_string();
    qCDebug(voip) << "Initialised" << version;
    g_free(version);

    // Required to register the qml types
    auto _sink = gst_element_factory_make("qmlglsink", nullptr);
    Q_ASSERT(_sink);
    gst_object_unref(_sink);
    return true;
}

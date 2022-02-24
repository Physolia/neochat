// SPDX-FileCopyrightText: 2020-2021 Nheko Authors
// SPDX-FileCopyrightText: 2021-2022 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "callmanager.h"

#include "controller.h"

#include <gst/gst.h>

#include "voiplogging.h"
#include <QDateTime>

#include <qcoro/qcorosignal.h>

CallManager::CallManager()
{
    init();
    connect(&Controller::instance(), &Controller::activeConnectionChanged, this, [this] {
        updateTurnServers();
    });
}

QCoro::Task<void> CallManager::updateTurnServers()
{
    qDebug() << m_cachedTurnUrisValidUntil << QDateTime::currentDateTime();
    if (m_cachedTurnUrisValidUntil > QDateTime::currentDateTime()) {
        co_return;
    }
    Controller::instance().activeConnection()->getTurnServers();

    auto servers = co_await qCoro(Controller::instance().activeConnection(), &Connection::turnServersChanged);
    m_cachedTurnUrisValidUntil = QDateTime::currentDateTime().addSecs(servers["ttl"].toInt());

    auto password = servers["password"].toString();
    auto username = servers["username"].toString();
    auto uris = servers["uris"].toArray();

    m_cachedTurnUris.clear();
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
        m_cachedTurnUris += scheme + QStringLiteral("://") + QUrl::toPercentEncoding(username) + QStringLiteral(":") + QUrl::toPercentEncoding(password)
            + QStringLiteral("@") + uri.mid(c + 1);
    }
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
    }
}

void CallManager::handleAnswer(NeoChatRoom *room, const Quotient::CallAnswerEvent *event)
{
    if (globalState() != OUTGOING) {
        qCDebug(voip) << "Not inviting; irrelevant answer";
        return;
    }
    // if this isn't our call, then we don't care
    if (event->callId() != m_callId) {
        return;
    }
    // if this is something we sent out...
    if (event->senderId() == room->localUser()->id()) {
        if (state() == CallSession::DISCONNECTED) {
            // this is us from another device, so we handled it from elsewhere
            //
            // TODO: Show the user that the call was answered on another device
            // TODO: Stop ringing
        } else {
            // this is the answer we sent out, so we don't need to handle it
        }
        return;
    }

    // if we're actually calling, accept the answer
    if (state() != CallSession::DISCONNECTED) {
        // TODO wait until candidates are here
        m_session->acceptAnswer(event->sdp(), m_incomingCandidates);
        return;
    }
    m_incomingCandidates.clear();
    setGlobalState(ACTIVE);
}

void CallManager::handleCandidates(NeoChatRoom *room, const Quotient::CallCandidatesEvent *event)
{
    if (event->senderId() == room->localUser()->id()) {
        return;
    }
    if (!m_callId.isEmpty() && event->callId() != m_callId) { // temp: don't accept candidates if there is a callId
        qCDebug(voip) << "Candidates not for this call; Skipping";
        return;
    }
    for (const auto &candidate : event->candidates()) {
        m_incomingCandidates +=
            Candidate{candidate.toObject()["candidate"].toString(), candidate.toObject()["sdpMLineIndex"].toInt(), candidate.toObject()["sdpMid"].toString()};
    }
}

void CallManager::handleInvite(NeoChatRoom *room, const Quotient::CallInviteEvent *event)
{
    if (globalState() != IDLE) {
        // TODO handle glare
        qCDebug(voip) << "Already in a call";
        return;
    }

    if (event->originTimestamp() < QDateTime::currentDateTime().addSecs(-60)) {
        return;
    }
    if (event->senderId() == room->localUser()->id()) {
        qCDebug(voip) << "Sent by this user";
        return;
    }
    setGlobalState(INCOMING);

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
    if (globalState() == IDLE) {
        qCDebug(voip) << "No call; irrelevant hangup";
        return;
    }

    if (event->senderId() == room->localUser()->id()) {
        return;
    }
    if (event->callId() != m_callId) {
        return;
    }
    if (m_session) {
        m_session->end();
    }
    setGlobalState(IDLE);
    Q_EMIT callEnded();
}

void CallManager::acceptCall()
{
    if (!hasInvite()) {
        return;
    }

    // TODO check plugins
    // TODO wait until candidates are here

    updateTurnServers();

    // TODO make video configurable
    //  change true to false if you don't have a camera
    m_session = CallSession::acceptCall(true, m_incomingSDP, m_incomingCandidates, m_cachedTurnUris, this);
    connect(m_session, &CallSession::stateChanged, this, [this] {
        Q_EMIT stateChanged();
        if (state() == CallSession::ICEFAILED) {
            Q_EMIT callEnded();
        }
    }); // TODO refactor away?
    m_incomingCandidates.clear();
    connectSingleShot(m_session, &CallSession::answerCreated, this, [=](const QString &sdp, const QVector<Candidate> &candidates) {
        m_room->answerCall(m_callId, sdp);
        qCDebug(voip) << "Sending Answer";
        m_room->sendCallCandidates(m_callId, candidatesToJson(candidates));
        qCDebug(voip) << "Sending Candidates";
        setGlobalState(ACTIVE);
    });
    m_hasInvite = false;
    Q_EMIT hasInviteChanged();
}

void CallManager::hangupCall()
{
    if (m_session) {
        m_session->end();
    }
    m_room->hangupCall(m_callId);
    setGlobalState(IDLE);
    Q_EMIT callEnded();
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
    if (!m_session) {
        return CallSession::DISCONNECTED;
    }
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

void CallManager::startCall(NeoChatRoom *room, bool sendVideo)
{
    if (m_session) {
        // Don't start calls if there already is one
        return;
    }
    if (room->users().size() != 2) {
        // Don't start calls if the room doesn't have exactly two members
        return;
    }

    // TODO check plugins?

    setLifetime(60000);
    setRoom(room);
    setRemoteUser(otherUser(room));

    updateTurnServers();

    setCallId(generateCallId());

    m_session = CallSession::startCall(sendVideo, m_cachedTurnUris, this);
    connect(m_session, &CallSession::stateChanged, this, [this] {
        Q_EMIT stateChanged();
        if (state() == CallSession::ICEFAILED) {
            Q_EMIT callEnded();
        }
    }); // TODO refactor away?

    connectSingleShot(m_session, &CallSession::offerCreated, this, [this](const QString &sdp, const QVector<Candidate> &candidates) {
        m_room->inviteCall(callId(), lifetime(), sdp);
        qCDebug(voip) << "Sending Invite";
        m_room->sendCallCandidates(callId(), candidatesToJson(candidates));
        qCDebug(voip) << "Sending Candidates";
    });
}

QString CallManager::generateCallId()
{
    return QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz");
}

void CallManager::setCallId(const QString &callId)
{
    m_callId = callId;
    Q_EMIT callIdChanged();
}

bool CallManager::isInviting() const
{
    return m_isInviting;
}

void CallManager::setMuted(bool muted)
{
    if (!m_session) {
        return;
    }
    m_session->setMuted(muted);
    Q_EMIT mutedChanged();
}

bool CallManager::muted() const
{
    if (!m_session) {
        return false;
    }
    return m_session->muted();
}

bool CallManager::init()
{
    qRegisterMetaType<Candidate>();
    qRegisterMetaType<QVector<Candidate>>();
    GError *error = nullptr;
    if (!gst_init_check(nullptr, nullptr, &error)) {
        QString strError;
        if (error) {
            strError += error->message;
            g_error_free(error);
        }
        qCCritical(voip) << "Failed to initialize GStreamer:" << strError;
        return false;
    }

    gchar *version = gst_version_string();
    qCDebug(voip) << "Initialised GStreamer: Version" << version;
    g_free(version);

    // Required to register the qml types
    auto _sink = gst_element_factory_make("qmlglsink", nullptr);
    Q_ASSERT(_sink);
    gst_object_unref(_sink);
    return true;
}

void CallManager::setLifetime(int lifetime)
{
    m_lifetime = lifetime;
    Q_EMIT lifetimeChanged();
}

void CallManager::setRoom(NeoChatRoom *room)
{
    m_room = room;
    Q_EMIT roomChanged();
}

void CallManager::setRemoteUser(NeoChatUser *user)
{
    m_remoteUser = user;
    Q_EMIT roomChanged();
}

NeoChatUser *CallManager::otherUser(NeoChatRoom *room)
{
    return dynamic_cast<NeoChatUser *>(room->users()[0]->id() == room->localUser()->id() ? room->users()[1] : room->users()[0]);
}

QJsonArray CallManager::candidatesToJson(const QVector<Candidate> &candidates) const
{
    QJsonArray candidatesJson;
    for (const auto &candidate : candidates) {
        candidatesJson += QJsonObject{{"candidate", candidate.candidate}, {"sdpMid", candidate.sdpMid}, {"sdpMLineIndex", candidate.sdpMLineIndex}};
    }
    return candidatesJson;
}

void CallManager::setGlobalState(GlobalState globalState)
{
    if (m_globalState == globalState) {
        return;
    }
    m_globalState = globalState;
    Q_EMIT globalStateChanged();
}

CallManager::GlobalState CallManager::globalState() const
{
    return m_globalState;
}

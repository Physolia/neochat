/* SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QString>
#include <QQuickItem>
#include <QMetaType>

#include <gst/gst.h>

struct Candidate
{
    QString candidate;
    int sdpMLineIndex;
    QString sdpMid;
};
Q_DECLARE_METATYPE(Candidate);
Q_DECLARE_METATYPE(QVector<Candidate>);

class CallSession : public QObject
{
    Q_OBJECT

public:
    enum CallType {
        VOICE,
        VIDEO,
        SCREEN,
    };

    enum State {
        DISCONNECTED,
        ICEFAILED,
        INITIATING,
        INITIATED,
        OFFERSENT,
        ANSWERSENT,
        CONNECTING,
        CONNECTED,
    };
    Q_ENUM(CallType);
    Q_ENUM(State);

    static CallSession &instance()
    {
        static CallSession _instance;
        return _instance;
    }

    void createOffer(CallType type);
    void acceptOffer(const QString &sdp);
    void acceptAnswer(const QString &sdp);
    void acceptICECandidates(const QVector<Candidate> &candidates);
    void end();

    void setTurnServers(QStringList servers);
    QQuickItem *getVideoItem() const;
    CallType calltype() const;

    bool isRemoteVideoReceiveOnly() const;
    bool isOffering() const;
    bool havePlugins(bool video) const;

    State state() const;

    // DON'T!
    void setState(State state);

Q_SIGNALS:
    void stateChanged();
    void offerCreated(const QString &sdp, const QVector<Candidate> &candidates);
    void answerCreated(const QString &sdp, const QVector<Candidate> &candidates);

private:
    CallSession();

    CallType m_callType;
    State m_state = DISCONNECTED;
    GstElement *m_webrtc = nullptr;
    bool m_isRemoteVideoReceiveOnly;
    bool m_isRemoteVideoSendOnly;
    bool m_isOffering;
    GstElement *m_pipe;
    QStringList m_turnServers;
    QQuickItem *m_videoItem;
    unsigned int m_busWatchId = 0;

    bool startPipeline(int opusPayloadType, int vp8PayloadType);
    bool createPipeline(int, int);
    void clear();
    bool addVideoPipeline(int vp8PayloadType);
};

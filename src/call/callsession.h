// SPDX-FileCopyrightText: 2021 Nheko Contributors
// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-FileCopyrightText: 2021 Carl Schwan <carl@carlschwan.eu>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMetaType>
#include <QObject>
#include <QQuickItem>
#include <QString>

#include <gst/gst.h>

#define OPUS_PAYLOAD_TYPE 111
#define VP8_PAYLOAD_TYPE 96

class CallDevices;

struct Candidate {
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
    Q_ENUM(State);

    static CallSession &instance()
    {
        static CallSession _instance;
        return _instance;
    }

    void startCall();
    void acceptOffer(const QString &sdp);
    void acceptAnswer(const QString &sdp);
    void acceptICECandidates(const QVector<Candidate> &candidates);
    void end();

    void setTurnServers(QStringList servers);
    QQuickItem *getVideoItem() const;

    bool isRemoteVideoReceiveOnly() const;
    bool isOffering() const;
    bool havePlugins(bool video) const;

    State state() const;

    void setState(State state);
    GstElement *m_webrtc = nullptr;

    QVector<Candidate> m_localCandidates;
    bool m_haveAudioStream = false;
    bool m_haveVideoStream = false;
    QString m_localSdp;

    void setMuted(bool muted);
    bool muted() const;
    GstElement *m_pipe;

    void setSendVideo(bool video);
    bool sendVideo() const;

Q_SIGNALS:
    void stateChanged();
    void offerCreated(const QString &sdp, const QVector<Candidate> &candidates);
    void answerCreated(const QString &sdp, const QVector<Candidate> &candidates);

private:
    CallSession();

    State m_state = DISCONNECTED;
    bool m_isRemoteVideoReceiveOnly;
    bool m_isRemoteVideoSendOnly;
    bool m_isOffering;
    QStringList m_turnServers;
    QQuickItem *m_videoItem;
    unsigned int m_busWatchId = 0;
    bool m_sendVideo = false;

    bool startPipeline();
    bool createPipeline();
    void clear();
    bool addVideoPipeline();
};

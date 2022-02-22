// SPDX-FileCopyrightText: 2021 Nheko Contributors
// SPDX-FileCopyrightText: 2021 Carl Schwan <carl@carlschwan.eu>
// SPDX-FileCopyrightText: 2021-2022 Tobias Fella <fella@posteo.de>
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

    Q_PROPERTY(CallSession::State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool isSendingVideo READ isSendingVideo NOTIFY isSendingVideoChanged)
    Q_PROPERTY(bool isReceivingVideo READ isReceivingVideo NOTIFY isReceivingVideoChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)

    static CallSession *startCall(bool sendVideo, const QStringList &turnUris, QObject *parent = nullptr);
    static CallSession *
    acceptCall(bool sendVideo, const QString &sdp, const QVector<Candidate> &candidates, const QStringList &turnUris, QObject *parent = nullptr);

    void acceptAnswer(const QString &sdp, const QVector<Candidate> &candidates);

    void end();

    void setTurnServers(QStringList servers);
    QQuickItem *getVideoItem() const;

    bool havePlugins(bool video) const;

    CallSession::State state() const;
    void setState(CallSession::State state);

    QVector<Candidate> m_localCandidates;
    bool m_haveAudioStream = false;
    bool m_haveVideoStream = false;
    QString m_localSdp;

    void setMuted(bool muted);
    bool muted() const;
    GstElement *m_pipe;

    void setIsSendingVideo(bool video);
    bool isSendingVideo() const;

    void setIsReceivingVideo(bool isReceivingVideo);
    bool isReceivingVideo() const;

    GstElement *m_webrtc = nullptr;
    bool m_isOffering = false;

Q_SIGNALS:
    void stateChanged();
    void offerCreated(const QString &sdp, const QVector<Candidate> &candidates);
    void answerCreated(const QString &sdp, const QVector<Candidate> &candidates);

    void isSendingVideoChanged();
    void isReceivingVideoChanged();

    void mutedChanged();

private:
    void acceptOffer(bool sendVideo, const QString &sdp, const QVector<Candidate> remoteCandidates);
    void createCall(bool sendVideo);

    void acceptCandidates(const QVector<Candidate> &candidates);

    CallSession::State m_state = CallSession::DISCONNECTED;
    QQuickItem *m_videoItem;
    unsigned int m_busWatchId = 0;
    bool m_isSendingVideo = false;
    bool m_isReceivingVideo = false;
    QStringList m_turnServers;

    bool startPipeline(bool sendVideo);
    bool createPipeline(bool sendVideo);
    bool addVideoPipeline();
    CallSession(QObject *parent = nullptr);
};

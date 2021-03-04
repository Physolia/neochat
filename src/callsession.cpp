/**
 * SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
 * SPDX-FileCopyrightText: 2020-2021 Nheko Authors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "callsession.h"

#include "calldevices.h"

#include <QThread>
#include <QDebug>

#include <gst/gst.h>

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>

#define STUN_SERVER "stun://turn.matrix.org:3478"

QString _localSdp;
QVector<Candidate> _localCandidates;
bool _haveAudioStream = false;
bool _haveVideoStream = false;
GstPad *_localPiPSinkPad = nullptr;
GstPad *_remotePiPSinkPad = nullptr;

struct KeyFrameRequestData
{
    GstElement *pipe = nullptr;
    GstElement *decodebin = nullptr;
    gint packetsLost = 0;
    guint timerid = 0;
    QString statsField;
} _keyFrameRequestData;

QPair<int, int> getResolution(GstPad *pad)
{
    QPair<int, int> ret;
    GstCaps *caps = gst_pad_get_current_caps(pad);
    const GstStructure *s = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(s, "width", &ret.first);
    gst_structure_get_int(s, "height", &ret.second);
    gst_caps_unref(caps);
    return ret;
}

QPair<int, int> getPiPDimensions(const QPair<int, int> &resolution, int fullWidth, double scaleFactor)
{
    int pipWidth  = fullWidth * scaleFactor;
    int pipHeight = static_cast<double>(resolution.second) / resolution.first * pipWidth;
    return qMakePair(pipWidth, pipHeight);
}


QPair<int, int> getResolution(GstElement *pipe, const gchar *elementName, const gchar *padName)
{
    GstElement *element = gst_bin_get_by_name(GST_BIN(pipe), elementName);
    GstPad *pad = gst_element_get_static_pad(element, padName);
    auto ret = getResolution(pad);
    gst_object_unref(pad);
    gst_object_unref(element);
    return ret;
}

void setLocalDescription(GstPromise *promise, gpointer webrtc)
{
    const GstStructure *reply = gst_promise_get_reply(promise);
    gboolean isAnswer = gst_structure_id_has_field(reply, g_quark_from_string("answer"));
    GstWebRTCSessionDescription *gstsdp = nullptr;
    gst_structure_get(reply, isAnswer ? "answer": "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &gstsdp, nullptr);
    gst_promise_unref(promise);
    g_signal_emit_by_name(webrtc, "set-local-description", gstsdp, nullptr);
    gchar *sdp = gst_sdp_message_as_text(gstsdp->sdp);
    _localSdp = QString(sdp);
    g_free(sdp);
    gst_webrtc_session_description_free(gstsdp);
    qDebug() << "Session: local description set:" << isAnswer << _localSdp;
}

bool contains(std::string_view str1, std::string_view str2)
{
    return std::search(str1.cbegin(), str1.cend(), str2.cbegin(), str2.cend(), [](unsigned char c1, unsigned char c2) {
        return std::tolower(c1) == std::tolower(c2);
    }) != str1.cend();
}

void createOffer(GstElement *webrtc)
{
    GstPromise *promise = gst_promise_new_with_change_func(setLocalDescription, webrtc, nullptr);
    g_signal_emit_by_name(webrtc, "create-offer", nullptr, promise);
}

void createAnswer(GstPromise *promise, gpointer webrtc)
{
    gst_promise_unref(promise);
    promise = gst_promise_new_with_change_func(setLocalDescription, webrtc, nullptr);
    g_signal_emit_by_name(webrtc, "create-answer", nullptr, promise);
}

void addLocalPiP(GstElement *pipe, const QPair<int, int> &videoCallSize)
{
    GstElement *tee = gst_bin_get_by_name(GST_BIN(pipe),"videosrctee");
    if(!tee) {
        return;
    }

    GstElement *queue = gst_element_factory_make("queue", nullptr);
    gst_bin_add(GST_BIN(pipe), queue);
    gst_element_link(tee, queue);
    gst_element_sync_state_with_parent(queue);
    gst_object_unref(tee);

    GstElement *compositor = gst_bin_get_by_name(GST_BIN(pipe), "compositor");
    _localPiPSinkPad = gst_element_get_request_pad(compositor, "sink_%u");
    g_object_set(_localPiPSinkPad, "zorder", 2, nullptr);

    bool isVideo = CallSession::instance().calltype() == CallSession::VIDEO;
    const gchar *element = isVideo ? "camerafilter" : "screenshare";
    const gchar *pad = isVideo ? "sink" : "src";
    auto resolution = getResolution(pipe, element, pad);
    auto pipSize = getPiPDimensions(resolution, videoCallSize.first, 0.25);
    qDebug() << "Session: PiP:" << pipSize.first << pipSize.second;
    g_object_set(_localPiPSinkPad, "width", pipSize.first, "height", pipSize.second, nullptr);
    gint offset = videoCallSize.first / 80;
    g_object_set(_localPiPSinkPad, "xpos", offset, "ypos", offset, nullptr);
    GstPad *srcpad = gst_element_get_static_pad(queue, "src");
    if(GST_PAD_LINK_FAILED(gst_pad_link(srcpad, _localPiPSinkPad))) {
        qDebug() << "Session: Failed to link local PiP elements";
    }
    gst_object_unref(srcpad);
    gst_object_unref(compositor);
}

bool getMediaAttributes(const GstSDPMessage *sdp, const char *mediaType, const char *encoding, int &payloadType, bool &receiveOnly, bool &sendOnly)
{
    payloadType = -1;
    receiveOnly = false;
    sendOnly = false;
    for(guint mlineIndex = 0; mlineIndex < gst_sdp_message_medias_len(sdp); mlineIndex++) {
        const GstSDPMedia *media = gst_sdp_message_get_media(sdp, mlineIndex);
        if(!strcmp(gst_sdp_media_get_media(media), mediaType)) {
            receiveOnly = gst_sdp_media_get_attribute_val(media, "recvonly") != nullptr;
            sendOnly = gst_sdp_media_get_attribute_val(media, "sendonly") != nullptr;
            const gchar *rtpval = nullptr;
            for(guint n = 0; n == 0 || rtpval; n++) {
                rtpval = gst_sdp_media_get_attribute_val_n(media, "rtpmap", n);
                if(rtpval && contains(rtpval, encoding)) {
                    payloadType = atoi(rtpval);
                    break;
                }
            }
            return true;
        }
    }
    return false;
}

GstWebRTCSessionDescription *parseSDP(const QString &sdp, GstWebRTCSDPType type)
{
    GstSDPMessage *message;
    gst_sdp_message_new(&message);
    if(gst_sdp_message_parse_buffer((guint8 *) sdp.toLatin1().data(), sdp.size(), message) == GST_SDP_OK) {
        return gst_webrtc_session_description_new(type, message);
    } else {
        //TODO: Error handling - failed to parse remote sdp
        gst_sdp_message_free(message);
        return nullptr;
    }
}

void addLocalICECandidate(GstElement *webrtc G_GNUC_UNUSED, guint mlineIndex, gchar *candidate, gpointer G_GNUC_UNUSED)
{
    _localCandidates += Candidate{candidate, static_cast<int>(mlineIndex), QString()};
}

void iceConnectionStateChanged(GstElement *webrtc, GParamSpec *pspec G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
    GstWebRTCICEConnectionState newState;
    g_object_get(webrtc, "ice-connection-state", &newState, nullptr);
    switch(newState) {
    case GST_WEBRTC_ICE_CONNECTION_STATE_CHECKING:
        qDebug() << "Session: GstWebRTCICEConnectionState -> Checking";
        CallSession::instance().setState(CallSession::CONNECTING);
        break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_FAILED:
        qDebug() << "Session: GstWebRTCICEConnectionState -> Failed";
        CallSession::instance().setState(CallSession::ICEFAILED);
        break;
    default:
        break;
    }
}

GstElement *newAudioSinkChain(GstElement *pipe)
{
    GstElement *queue = gst_element_factory_make("queue", nullptr);
    GstElement *convert = gst_element_factory_make("audioconvert", nullptr);
    GstElement *resample = gst_element_factory_make("audioresample", nullptr);
    GstElement *sink = gst_element_factory_make("autoaudiosink", nullptr);
    gst_bin_add_many(GST_BIN(pipe), queue, convert, resample, sink, nullptr);
    gst_element_link_many(queue, convert, resample, sink, nullptr);
    gst_element_sync_state_with_parent(queue);
    gst_element_sync_state_with_parent(convert);
    gst_element_sync_state_with_parent(resample);
    gst_element_sync_state_with_parent(sink);
    return queue;
}

GstElement *newVideoSinkChain(GstElement *pipe)
{
    // use compositor for now; acceleration needs investigation
    GstElement *queue = gst_element_factory_make("queue", nullptr);
    GstElement *compositor = gst_element_factory_make("compositor", "compositor");
    GstElement *glupload = gst_element_factory_make("glupload", nullptr);
    GstElement *glcolorconvert = gst_element_factory_make("glcolorconvert", nullptr);
    GstElement *qmlglsink = gst_element_factory_make("qmlglsink", nullptr);
    GstElement *glsinkbin = gst_element_factory_make("glsinkbin", nullptr);
    g_object_set(compositor, "background", 1, nullptr);
    g_object_set(qmlglsink, "widget", CallSession::instance().getVideoItem(), nullptr);
    g_object_set(glsinkbin, "sink", qmlglsink, nullptr);
    gst_bin_add_many(GST_BIN(pipe), queue, compositor, glupload, glcolorconvert, glsinkbin, nullptr);
    gst_element_link_many(queue, compositor, glupload, glcolorconvert, glsinkbin, nullptr);
    gst_element_sync_state_with_parent(queue);
    gst_element_sync_state_with_parent(compositor);
    gst_element_sync_state_with_parent(glupload);
    gst_element_sync_state_with_parent(glcolorconvert);
    gst_element_sync_state_with_parent(glsinkbin);
    return queue;
}

void sendKeyFrameRequest()
{
    GstPad *sinkpad = gst_element_get_static_pad(_keyFrameRequestData.decodebin, "sink");
    if (!gst_pad_push_event(sinkpad, gst_event_new_custom(GST_EVENT_CUSTOM_UPSTREAM, gst_structure_new_empty("GstForceKeyUnit")))) {
        qDebug() << "Session: key frame request failed";
    } else {
        qDebug() << "Session: sent key frame request";
    }
    gst_object_unref(sinkpad);
}

void _testPacketLoss(GstPromise *promise, gpointer G_GNUC_UNUSED)
{
    const GstStructure *reply = gst_promise_get_reply(promise);
    gint packetsLost = 0;
    GstStructure *rtpStats;
    if (!gst_structure_get(reply, _keyFrameRequestData.statsField.toLatin1().data(), GST_TYPE_STRUCTURE, &rtpStats, nullptr)) {
        qDebug() << "Session: get-stats: no field:" << _keyFrameRequestData.statsField;
        gst_promise_unref(promise);
        return;
    }
    gst_structure_get_int(rtpStats, "packets-lost", &packetsLost);
    gst_structure_free(rtpStats);
    gst_promise_unref(promise);
    if (packetsLost > _keyFrameRequestData.packetsLost) {
        qDebug() << "Session: inbound video lost packet count:" << packetsLost;
        _keyFrameRequestData.packetsLost = packetsLost;
        sendKeyFrameRequest();
    }
}

void addRemotePiP(GstElement *pipe)
{
    if(_remotePiPSinkPad) {
        auto camRes = getResolution(pipe, "camerafilter", "sink");
        auto shareRes = getResolution(pipe, "screenshare", "src");
        auto pipSize = getPiPDimensions(camRes, shareRes.first, 0.2);
        qDebug() << "Session: Screenshare PiP:" << pipSize.first << pipSize.second;

        gint offset = shareRes.first / 100;
        g_object_set(_remotePiPSinkPad, "zorder", 2, nullptr);
        g_object_set(_remotePiPSinkPad, "width", pipSize.first, "height", pipSize.second, nullptr);
        g_object_set(_remotePiPSinkPad, "xpos", shareRes.first - pipSize.first - offset, "ypos", shareRes.second - pipSize.second - offset, nullptr);
    }
}

gboolean testPacketLoss(gpointer G_GNUC_UNUSED)
{
    if (_keyFrameRequestData.pipe) {
        GstElement *webrtc = gst_bin_get_by_name(GST_BIN(_keyFrameRequestData.pipe), "webrtcbin");
        GstPromise *promise = gst_promise_new_with_change_func(_testPacketLoss, nullptr, nullptr);
        g_signal_emit_by_name(webrtc, "get-stats", nullptr, promise);
        gst_object_unref(webrtc);
        return TRUE;
    }
    return FALSE;
}

void addLocalVideo(GstElement *pipe)
{
    GstElement *queue = newVideoSinkChain(pipe);
    GstElement *tee = gst_bin_get_by_name(GST_BIN(pipe), "videosrctee");
    GstPad *srcpad = gst_element_get_request_pad(tee, "src_%u");
    GstPad *sinkpad = gst_element_get_static_pad(queue, "sink");
    if (GST_PAD_LINK_FAILED(gst_pad_link(srcpad, sinkpad))) {
        qDebug() << "Session: failed to link videosrctee -> video sink chain";
    }
    gst_object_unref(srcpad);
}

void linkNewPad(GstElement *decodebin, GstPad *newpad, GstElement *pipe)
{
    GstPad *sinkpad = gst_element_get_static_pad(decodebin, "sink");
    GstCaps *sinkcaps = gst_pad_get_current_caps(sinkpad);
    const GstStructure *structure = gst_caps_get_structure(sinkcaps, 0);

    gchar *mediaType = nullptr;
    guint ssrc = 0;
    gst_structure_get(structure, "media", G_TYPE_STRING, &mediaType, "ssrc", G_TYPE_UINT, &ssrc, nullptr);
    gst_caps_unref(sinkcaps);
    gst_object_unref(sinkpad);

    CallSession *session = &CallSession::instance();
    GstElement *queue = nullptr;
    if(!strcmp(mediaType, "audio")) {
        qDebug() << "Session: Receiving incoming audio stream";
        _haveAudioStream = true;
        queue = newAudioSinkChain(pipe);
    } else if(!strcmp(mediaType, "video")) {
        qDebug() << "Session: Receiving incoming video stream";
        if(!session->getVideoItem()) {
            g_free(mediaType);
            qDebug() << "Session: video call item not set";
            //TODO: Error handling
            return;
        }
        _haveVideoStream = true;
        _keyFrameRequestData.statsField = QStringLiteral("rtp-inbound-stream-stats_") + QString::number(ssrc);
        queue = newVideoSinkChain(pipe);
        auto videoCallSize = getResolution(newpad);
        qDebug() << "Session: Incoming video resolution:" << videoCallSize.first << videoCallSize.second;
        addLocalPiP(pipe, videoCallSize);
    } else {
        g_free(mediaType);
        qDebug() << "Session: Unknown pad type:" << GST_PAD_NAME(newpad);
        return;
    }

    GstPad *queuepad = gst_element_get_static_pad(queue, "sink");
    if(queuepad) {
        if(GST_PAD_LINK_FAILED(gst_pad_link(newpad, queuepad))) {
            qDebug() << "Session: Unable to link new pad";
            //TODO: Error handling
        } else {
            if(session->calltype() != CallSession::VIDEO || (_haveAudioStream && (_haveVideoStream || session->isRemoteVideoReceiveOnly()))) {
                session->setState(CallSession::CONNECTED);
                if(_haveVideoStream) {
                    _keyFrameRequestData.pipe = pipe;
                    _keyFrameRequestData.decodebin = decodebin;
                    _keyFrameRequestData.timerid = g_timeout_add_seconds(3, testPacketLoss, nullptr);
                }
                addRemotePiP(pipe);
                if(session->isRemoteVideoReceiveOnly()) {
                    addLocalVideo(pipe);
                }
            }
        }
        gst_object_unref(queuepad);
    }
    g_free(mediaType);
}

void setWaitForKeyFrame(GstBin *decodebin G_GNUC_UNUSED, GstElement *element, gpointer G_GNUC_UNUSED)
{
    if (!strcmp(gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(gst_element_get_factory(element))), "rtpvp8depay")) {
        g_object_set(element, "wait-for-keyframe", TRUE, nullptr);
    }
}

void addDecodeBin(GstElement *webrtc G_GNUC_UNUSED, GstPad *newpad, GstElement *pipe)
{
    if(GST_PAD_DIRECTION(newpad) != GST_PAD_SRC) {
        return;
    }

    qDebug() << "Session: Receiving incoming stream";
    GstElement *decodebin = gst_element_factory_make("decodebin", nullptr);
    //Investigate hardware, see nheko source
    g_object_set(decodebin, "force-sw-decoders", TRUE, nullptr);
    g_signal_connect(decodebin, "pad-added", G_CALLBACK(linkNewPad), pipe);
    g_signal_connect(decodebin, "element-added", G_CALLBACK(setWaitForKeyFrame), nullptr);
    gst_bin_add(GST_BIN(pipe), decodebin);
    gst_element_sync_state_with_parent(decodebin);
    GstPad *sinkpad = gst_element_get_static_pad(decodebin, "sink");
    if(GST_PAD_LINK_FAILED(gst_pad_link(newpad, sinkpad))) {
        //TODO: Error handling - unable to link decodebin
        qDebug() << "Session: Unable to link decodebin";
    }
    gst_object_unref(sinkpad);
}

void iceGatheringStateChanged(GstElement *webrtc, GParamSpec *pspec G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
    GstWebRTCICEGatheringState newState;
    g_object_get(webrtc, "ice-gathering-state", &newState, nullptr);
    if(newState == GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE) {
        qDebug() << "Session: GstWebRTCICEGatheringState -> Complete";
        if(CallSession::instance().isOffering()) {
            Q_EMIT CallSession::instance().offerCreated(_localSdp, _localCandidates);
            CallSession::instance().setState(CallSession::OFFERSENT);
        } else {
            Q_EMIT CallSession::instance().answerCreated(_localSdp, _localCandidates);
            CallSession::instance().setState(CallSession::ANSWERSENT);
        }
    }
}

gboolean newBusMessage(GstBus *bus G_GNUC_UNUSED, GstMessage *msg, gpointer user_data)
{
    CallSession *session = static_cast<CallSession *>(user_data);
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
        qDebug() << "WebRTC: end of stream";
        //TODO: Error handling
        session->end();
        break;
    case GST_MESSAGE_ERROR:
        GError *error;
        gchar *debug;
        gst_message_parse_error(msg, &error, &debug);
        qDebug() << "WebRTC: error from element:" << GST_OBJECT_NAME(msg->src) << error->message;
        //TODO: Error handling
        g_clear_error(&error);
        g_free(debug);
        session->end();
        break;
    default:
        break;
    }
    return TRUE;
}

CallSession::CallSession()
{
    qRegisterMetaType<CallSession::State>();
    qRegisterMetaType<CallSession::CallType>();
    qRegisterMetaType<Candidate>();
    qRegisterMetaType<QVector<Candidate>>();
}

void CallSession::acceptAnswer(const QString &sdp)
{
    qDebug() << "Session: Received answer";
    if(m_state != CallSession::OFFERSENT) {
        return;
    }

    GstWebRTCSessionDescription *answer = parseSDP(sdp, GST_WEBRTC_SDP_TYPE_ANSWER);
    if(!answer) {
        end();
        return;
    }

    if(m_callType != CallSession::VOICE) {
        int unused;
        if(!getMediaAttributes(answer->sdp, "video", "vp8", unused, m_isRemoteVideoReceiveOnly, m_isRemoteVideoSendOnly)) {
            m_isRemoteVideoReceiveOnly = true;
        }
    }
    g_signal_emit_by_name(m_webrtc, "set-remote-description", answer, nullptr);
    return;
}

void CallSession::acceptOffer(const QString& sdp)
{
    qDebug() << "Session: Received offer:" << sdp;
    if(m_state != CallSession::DISCONNECTED) {
        return;
    }

    m_isOffering = false;

    clear();
    GstWebRTCSessionDescription *offer = parseSDP(sdp, GST_WEBRTC_SDP_TYPE_OFFER);
    if(!offer) {
        //TODO: Error handling
        return;
    }

    int opusPayloadType;
    bool receiveOnly;
    bool sendOnly;
    if(getMediaAttributes(offer->sdp, "audio", "opus", opusPayloadType, receiveOnly, sendOnly)) {
        if(opusPayloadType == -1) {
            //TODO: Error handling - no OPUS encoding
            gst_webrtc_session_description_free(offer);
            return;
        }
    } else {
        //TODO: Error handling - no audio media
        gst_webrtc_session_description_free(offer);
        return;
    }

    int vp8PayloadType;
    bool isVideo = getMediaAttributes(offer->sdp, "video", "vp8", vp8PayloadType, m_isRemoteVideoReceiveOnly, m_isRemoteVideoSendOnly);
    if(isVideo && vp8PayloadType == -1) {
        //TODO: Error handling - no vp8 encoding;
        gst_webrtc_session_description_free(offer);
        return;
    }
    if(!startPipeline(opusPayloadType, vp8PayloadType)) {
        gst_webrtc_session_description_free(offer);
        return;
    }
    QThread::msleep(200);

    GstPromise *promise = gst_promise_new_with_change_func(createAnswer, m_webrtc, nullptr);
    g_signal_emit_by_name(m_webrtc, "set-remote-description", offer, promise);
    gst_webrtc_session_description_free(offer);
}

void CallSession::createOffer(CallType type)
{
    clear();
    m_callType = type;
    m_isOffering = true;

    constexpr int opusPayloadType = 111;
    constexpr int vp8PayloadType = 96;
    if(!startPipeline(opusPayloadType, vp8PayloadType)) {
        //TODO: Error handling
    }
}
void CallSession::clear()
{
    m_callType = CallSession::VOICE;
    m_isOffering = false;
    m_isRemoteVideoReceiveOnly = false;
    m_isRemoteVideoSendOnly = false;
    m_videoItem = nullptr;
    m_pipe = nullptr;
    m_webrtc = nullptr;
    m_state = CallSession::DISCONNECTED;
    m_busWatchId = 0;
    //TODO m_shareWindowId = 0;
    //TODO m_haveAudioStream = false;
    //TODO m_haveVideoStream = false;
    _localPiPSinkPad = nullptr;
    _remotePiPSinkPad = nullptr;
    _localSdp.clear();
    _localCandidates.clear();
}

bool CallSession::startPipeline(int opusPayloadType, int vp8PayloadType)
{
    if(m_state != State::DISCONNECTED) {
        return false;
    }
    m_state = CallSession::INITIATING;
    Q_EMIT stateChanged();

    if(!createPipeline(opusPayloadType, vp8PayloadType)) {
        end();
        return false;
    }
    m_webrtc = gst_bin_get_by_name(GST_BIN(m_pipe), "webrtcbin");
    if(false /*TODO: CHECK USE STUN*/) {
        qDebug() << "Session: Setting STUN server:" << STUN_SERVER;
        g_object_set(m_webrtc, "stun-server", STUN_SERVER, nullptr);
    }

    for(const auto &uri : m_turnServers) {
        qDebug() << "Session: Setting turn server:" << uri;
            gboolean udata;
            g_signal_emit_by_name(m_webrtc, "add-turn-server", uri.toLatin1().data(), (gpointer)(&udata));
    }

    if(m_turnServers.empty()) {
        qDebug() << "Session: No TURN servers provided";
    }

    if(m_isOffering) {
        g_signal_connect(m_webrtc, "on-negotiation-needed", G_CALLBACK(::createOffer), nullptr);
    }

    g_signal_connect(m_webrtc, "on-ice-candidate", G_CALLBACK(addLocalICECandidate), nullptr);
    g_signal_connect(m_webrtc, "notify::ice-connection-state", G_CALLBACK(iceConnectionStateChanged), nullptr);

    gst_element_set_state(m_pipe, GST_STATE_READY);
    g_signal_connect(m_webrtc, "pad-added", G_CALLBACK(addDecodeBin), m_pipe);

    g_signal_connect(m_webrtc, "notify::ice-gathering-state", G_CALLBACK(iceGatheringStateChanged), nullptr);
    gst_object_unref(m_webrtc);

    qDebug() << "Random debug statement #1";

    GstStateChangeReturn ret = gst_element_set_state(m_pipe, GST_STATE_PLAYING);
    if(ret == GST_STATE_CHANGE_FAILURE) {
        //TODO: Error handling - unable to start pipeline
        qDebug() << "unable to start pipeline";
        end();
        return false;
    }

    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipe));
    m_busWatchId = gst_bus_add_watch(bus, newBusMessage, this);
    gst_object_unref(bus);

    m_state = CallSession::INITIATED;
    Q_EMIT stateChanged();

    return true;
}

void CallSession::end()
{
    qDebug() << "Session: ending";
    _keyFrameRequestData = KeyFrameRequestData{};
    if(m_pipe) {
        gst_element_set_state(m_pipe, GST_STATE_NULL);
        gst_object_unref(m_pipe);
        m_pipe = nullptr;
        if(m_busWatchId) {
            g_source_remove(m_busWatchId);
            m_busWatchId = 0;
        }
    }
    clear();
    if(m_state != CallSession::DISCONNECTED) {
        m_state = CallSession::DISCONNECTED;
            Q_EMIT stateChanged();
    }
}

bool CallSession::createPipeline(int opusPayloadType, int vp8PayloadType)
{
    GstDevice *device = CallDevices::instance().audioDevicesModel()->currentDevice();
    if(!device) {
        return false;
    }

    GstElement *source = gst_device_create_element(device, nullptr);
    GstElement *volume = gst_element_factory_make("volume", "srclevel");
    GstElement *convert = gst_element_factory_make("audioconvert", nullptr);
    GstElement *resample = gst_element_factory_make("audioresample", nullptr);
    GstElement *queue1 = gst_element_factory_make("queue", nullptr);
    GstElement *opusenc = gst_element_factory_make("opusenc", nullptr);
    GstElement *rtp = gst_element_factory_make("rtpopuspay", nullptr);
    GstElement *queue2 = gst_element_factory_make("queue", nullptr);
    GstElement *capsfilter = gst_element_factory_make("capsfilter", nullptr);

    GstCaps *rtpcaps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, "audio", "encoding-name", G_TYPE_STRING, "OPUS", "payload", G_TYPE_INT, opusPayloadType, nullptr);
    g_object_set(capsfilter, "caps", rtpcaps, nullptr);
    gst_caps_unref(rtpcaps);

    GstElement *webrtcbin = gst_element_factory_make("webrtcbin", "webrtcbin");
    g_object_set(webrtcbin, "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE, nullptr);

    m_pipe = gst_pipeline_new(nullptr);
    gst_bin_add_many(GST_BIN(m_pipe), source, volume, convert, resample, queue1, opusenc, rtp, queue2, capsfilter, webrtcbin, nullptr);

    if(!gst_element_link_many(source, volume, convert, resample, queue1, opusenc, rtp, queue2, capsfilter, webrtcbin, nullptr)) {
        //TODO: Error handling - failed to link pipeline
        qDebug() << "Session: failed to link pipeline";
        return false;
    }

    return m_callType != CallSession::VIDEO || m_isRemoteVideoSendOnly ? true : addVideoPipeline(vp8PayloadType);
}

bool CallSession::addVideoPipeline(int vp8PayloadType)
{
    // allow incoming video calls despite localUser having no webcam
    if (m_callType != CallSession::VIDEO && !CallDevices::instance().videoDevicesModel()->haveCamera()) {
        return !m_isOffering;
    }

    //TODO auto settings = ChatPage::instance()->userSettings();
    GstElement *camerafilter = nullptr;
    GstElement *videoconvert = gst_element_factory_make("videoconvert", nullptr);
    GstElement *tee = gst_element_factory_make("tee", "videosrctee");
    gst_bin_add_many(GST_BIN(m_pipe), videoconvert, tee, nullptr);
    if (m_callType != CallSession::VIDEO || (false/*settings->screenSharePiP()TODO*/ && CallDevices::instance().videoDevicesModel()->haveCamera())) {
        QPair<int, int> resolution;
        QPair<int, int> frameRate;
        GstDevice *device = CallDevices::instance().videoDevicesModel()->videoDevice(resolution, frameRate);
        if (!device) {
            return false;
        }
        GstElement *camera = gst_device_create_element(device, nullptr);
        GstCaps *caps = gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, resolution.first, "height", G_TYPE_INT, resolution.second, "framerate", GST_TYPE_FRACTION, frameRate.first, frameRate.second, nullptr);
        camerafilter = gst_element_factory_make("capsfilter", "camerafilter");
        g_object_set(camerafilter, "caps", caps, nullptr);
        gst_caps_unref(caps);

        gst_bin_add_many(GST_BIN(m_pipe), camera, camerafilter, nullptr);
        if (!gst_element_link_many(camera, videoconvert, camerafilter, nullptr)) {
            qDebug() << "Session: failed to link camera elements";
            //TODO: Error handling
            return false;
        }
        if (m_callType == CallType::VIDEO && !gst_element_link(camerafilter, tee)) {
            qDebug() << "Session: failed to link camerafilter -> tee";
            //TODO: Error handling
            return false;
        }
    }

    if (m_callType == CallType::SCREEN) {
        qDebug() << "Session: screen share frame rate:" << 30; //TODO << settings->screenShareFrameRate();
        qDebug() << "Session: screen share picture-in-picture:" << false; //TODO << settings->screenSharePiP();
        qDebug() << "Session: screen share request remote camera:" << false; //TODO << settings->screenShareRemoteVideo();
        qDebug() << "Session: screen share hide mouse cursor:" << false; //TODO << settings->screenShareHideCursor();

        GstElement *ximagesrc = gst_element_factory_make("ximagesrc", "screenshare");
        if (!ximagesrc) {
            qDebug() << "Session: failed to create ximagesrc";
            //TODO: Error handling
            return false;
        }
        g_object_set(ximagesrc, "use-damage", FALSE, nullptr);
        g_object_set(ximagesrc, "xid", 1 /*TODO shareWindowId_*/, nullptr);
        g_object_set(ximagesrc, "show-pointer", false /*TODO !settings->screenShareHideCursor()*/, nullptr);

        GstCaps *caps = gst_caps_new_simple("video/x-raw", "framerate", GST_TYPE_FRACTION, 30 /*TODO settings->screenShareFrameRate()*/, 1, nullptr);
        GstElement *capsfilter = gst_element_factory_make("capsfilter", nullptr);
        g_object_set(capsfilter, "caps", caps, nullptr);
        gst_caps_unref(caps);
        gst_bin_add_many(GST_BIN(m_pipe), ximagesrc, capsfilter, nullptr);

        if (false /*TODO settings->screenSharePiP()*/ && CallDevices::instance().videoDevicesModel()->haveCamera()) {
            GstElement *compositor = gst_element_factory_make("compositor", nullptr);
            g_object_set(compositor, "background", 1, nullptr);
            gst_bin_add(GST_BIN(m_pipe), compositor);
            if (!gst_element_link_many(ximagesrc, compositor, capsfilter, tee, nullptr)) {
                qDebug() << "Session: failed to link screen share elements";
                return false;
            }

            GstPad *srcpad = gst_element_get_static_pad(camerafilter, "src");
            _remotePiPSinkPad = gst_element_get_request_pad(compositor, "sink_%u");
            if (GST_PAD_LINK_FAILED(gst_pad_link(srcpad, _remotePiPSinkPad))) {
                qDebug() << "Session: failed to link camerafilter -> compositor";
                gst_object_unref(srcpad);
                return false;
            }
            gst_object_unref(srcpad);
        } else if (!gst_element_link_many(ximagesrc, videoconvert, capsfilter, tee, nullptr)) {
            qDebug() << "Session: failed to link screen share elements";
            return false;
        }
    }

    GstElement *queue = gst_element_factory_make("queue", nullptr);
    GstElement *vp8enc = gst_element_factory_make("vp8enc", nullptr);
    g_object_set(vp8enc, "deadline", 1, nullptr);
    g_object_set(vp8enc, "error-resilient", 1, nullptr);
    GstElement *rtpvp8pay = gst_element_factory_make("rtpvp8pay", nullptr);
    GstElement *rtpqueue = gst_element_factory_make("queue", nullptr);
    GstElement *rtpcapsfilter = gst_element_factory_make("capsfilter", nullptr);
    GstCaps *rtpcaps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, "video", "encoding-name", G_TYPE_STRING, "VP8", "payload", G_TYPE_INT, vp8PayloadType, nullptr);
    g_object_set(rtpcapsfilter, "caps", rtpcaps, nullptr);
    gst_caps_unref(rtpcaps);

    gst_bin_add_many(GST_BIN(m_pipe), queue, vp8enc, rtpvp8pay, rtpqueue, rtpcapsfilter, nullptr);

    GstElement *webrtcbin = gst_bin_get_by_name(GST_BIN(m_pipe), "webrtcbin");
    if (!gst_element_link_many(tee, queue, vp8enc, rtpvp8pay, rtpqueue, rtpcapsfilter, webrtcbin, nullptr)) {
        qDebug() << "WebRTC: failed to link rtp video elements";
        gst_object_unref(webrtcbin);
        return false;
    }

    if (m_callType == CallType::SCREEN && false /*TODO !ChatPage::instance()->userSettings()->screenShareRemoteVideo()*/) {
        GArray *transceivers;
        g_signal_emit_by_name(webrtcbin, "get-transceivers", &transceivers);
        GstWebRTCRTPTransceiver *transceiver = g_array_index(transceivers, GstWebRTCRTPTransceiver *, 1);
        transceiver->direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
        g_array_unref(transceivers);
    }

    gst_object_unref(webrtcbin);
    return true;
}

void CallSession::setTurnServers(QStringList servers)
{
    m_turnServers = servers;
}

void CallSession::setState(CallSession::State state)
{
    m_state = state;
    Q_EMIT stateChanged();
}

QQuickItem *CallSession::getVideoItem() const
{
    return m_videoItem;
}

bool CallSession::isRemoteVideoReceiveOnly() const
{
    return m_isRemoteVideoReceiveOnly;
}

bool CallSession::isOffering() const
{
    return m_isOffering;
}

void CallSession::acceptICECandidates(const QVector<Candidate>& candidates)
{
    if(m_state >= State::INITIATED) {
        for(const auto &c : candidates) {
            qDebug() << "Remote candidate:" << c.candidate << c.sdpMid << c.sdpMLineIndex;
            if(!c.candidate.isEmpty()) {
                g_signal_emit_by_name(m_webrtc, "add-ice-candidate", c.sdpMLineIndex, c.candidate.toLatin1().data());
            }
        }
    }
}

CallSession::CallType CallSession::calltype() const
{
    return m_callType;
}

CallSession::State CallSession::state() const
{
    return m_state;
}

bool CallSession::havePlugins(bool video) const
{
    if(video) {
        const gchar *videoPlugins[] = {"compositor", "opengl", "qmlgl", "rtp", "videoconvert", "vpx", nullptr};
         GstRegistry *registry = gst_registry_get();
        for(guint i = 0; i < g_strv_length((gchar **) videoPlugins); i++) {
            GstPlugin *plugin = gst_registry_find_plugin(registry, videoPlugins[i]);
            if(!plugin) {
                qDebug() << "Missing gstreamer plugin:" << videoPlugins[i];
                return false;
            }
            gst_object_unref(plugin);
        }
        GstElement *qmlglsink = gst_element_factory_make("qmlglsink", nullptr);
        gst_object_unref(qmlglsink);
    }

    const gchar *voicePlugins[] = {"audioconvert", "audioresample", "autodetect", "dtls", "nice", "opus", "playback", "rtpmanager", "srtp", "volume", "webrtc", nullptr};
    GstRegistry *registry = gst_registry_get();
    for(guint i = 0; i < g_strv_length((gchar **) voicePlugins); i++) {
        GstPlugin *plugin = gst_registry_find_plugin(registry, voicePlugins[i]);
        if(!plugin) {
            qDebug() << "Missing gstreamer plugin:" << voicePlugins[i];
            return false;
        }
        gst_object_unref(plugin);
    }

    qDebug() << "Session: All plugins installed";
    return true;
}




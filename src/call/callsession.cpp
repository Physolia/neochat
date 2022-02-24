// SPDX-FileCopyrightText: 2021 Nheko Contributors
// SPDX-FileCopyrightText: 2021 Carl Schwan <carl@carlschwan.eu>
// SPDX-FileCopyrightText: 2021-2022 Tobias Fella <fella@posteo.de>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "callsession.h"

#include "calldevices.h"

#include <QDebug>
#include <QThread>

#include <gst/gst.h>

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>

#include "callmanager.h"

#include "voiplogging.h"

#include "audiosources.h"
#include "videosources.h"

#define STUN_SERVER "stun://turn.matrix.org:3478" // TODO make STUN server configurable

struct KeyFrameRequestData {
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

QPair<int, int> getResolution(GstElement *pipe, const gchar *elementName, const gchar *padName)
{
    GstElement *element = gst_bin_get_by_name(GST_BIN(pipe), elementName);
    GstPad *pad = gst_element_get_static_pad(element, padName);
    auto ret = getResolution(pad);
    gst_object_unref(pad);
    gst_object_unref(element);
    return ret;
}

void setLocalDescription(GstPromise *promise, gpointer user_data)
{
    qDebug() << "Session: Setting local description";
    auto instance = static_cast<CallSession *>(user_data);
    const GstStructure *reply = gst_promise_get_reply(promise);
    gboolean isAnswer = gst_structure_id_has_field(reply, g_quark_from_string("answer"));
    GstWebRTCSessionDescription *gstsdp = nullptr;
    gst_structure_get(reply, isAnswer ? "answer" : "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &gstsdp, nullptr);
    gst_promise_unref(promise);
    g_signal_emit_by_name(instance->m_webrtc, "set-local-description", gstsdp, nullptr);
    gchar *sdp = gst_sdp_message_as_text(gstsdp->sdp);
    instance->m_localSdp = QString(sdp);
    g_free(sdp);
    gst_webrtc_session_description_free(gstsdp);
    qCDebug(voip) << "Session: local description set:" << isAnswer << instance->m_localSdp;
}

bool contains(std::string_view str1, std::string_view str2)
{
    return std::search(str1.cbegin(),
                       str1.cend(),
                       str2.cbegin(),
                       str2.cend(),
                       [](unsigned char c1, unsigned char c2) {
                           return std::tolower(c1) == std::tolower(c2);
                       })
        != str1.cend();
}

void createOffer(GstElement *webrtc, CallSession *session)
{
    qDebug() << "Session: Creating Offer";
    auto promise = gst_promise_new_with_change_func(setLocalDescription, session, nullptr);
    g_signal_emit_by_name(webrtc, "create-offer", nullptr, promise);
}

void createAnswer(GstPromise *promise, gpointer user_data)
{
    qDebug() << "Session: Creating Answer";
    auto instance = static_cast<CallSession *>(user_data);
    gst_promise_unref(promise);
    promise = gst_promise_new_with_change_func(setLocalDescription, instance, nullptr);
    g_signal_emit_by_name(instance->m_webrtc, "create-answer", nullptr, promise);
}

bool getMediaAttributes(const GstSDPMessage *sdp, const char *mediaType, const char *encoding, int &payloadType, bool &receiveOnly, bool &sendOnly)
{
    payloadType = -1;
    receiveOnly = false;
    sendOnly = false;
    for (guint mlineIndex = 0; mlineIndex < gst_sdp_message_medias_len(sdp); mlineIndex++) {
        const GstSDPMedia *media = gst_sdp_message_get_media(sdp, mlineIndex);
        if (!strcmp(gst_sdp_media_get_media(media), mediaType)) {
            receiveOnly = gst_sdp_media_get_attribute_val(media, "recvonly") != nullptr;
            sendOnly = gst_sdp_media_get_attribute_val(media, "sendonly") != nullptr;
            const gchar *rtpval = nullptr;
            for (guint n = 0; n == 0 || rtpval; n++) {
                rtpval = gst_sdp_media_get_attribute_val_n(media, "rtpmap", n);
                if (rtpval && contains(rtpval, encoding)) {
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
    if (gst_sdp_message_parse_buffer((guint8 *)sdp.toLatin1().data(), sdp.size(), message) == GST_SDP_OK) {
        return gst_webrtc_session_description_new(type, message);
    } else {
        qCCritical(voip) << "Failed to parse remote SDP";
        gst_sdp_message_free(message);
        return nullptr;
    }
}

void addLocalICECandidate(GstElement *webrtc G_GNUC_UNUSED, guint mlineIndex, gchar *candidate, gpointer user_data)
{
    qCDebug(voip) << "Adding local ICE Candidates";
    auto instance = static_cast<CallSession *>(user_data);
    Q_ASSERT(instance);
    instance->m_localCandidates += Candidate{candidate, static_cast<int>(mlineIndex), QString()};
}

void iceConnectionStateChanged(GstElement *webrtc, GParamSpec *pspec G_GNUC_UNUSED, gpointer user_data)
{
    auto instance = static_cast<CallSession *>(user_data);
    Q_ASSERT(instance);
    GstWebRTCICEConnectionState newState;
    g_object_get(webrtc, "ice-connection-state", &newState, nullptr);
    switch (newState) {
    case GST_WEBRTC_ICE_CONNECTION_STATE_NEW:
        qCDebug(voip) << "GstWebRTCICEConnectionState -> New";
        instance->setState(CallSession::CONNECTING);
        break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_CHECKING:
        qCDebug(voip) << "GstWebRTCICEConnectionState -> Checking";
        instance->setState(CallSession::CONNECTING);
        break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_CONNECTED:
        qCDebug(voip) << "GstWebRTCICEConnectionState -> Connected";
        break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_COMPLETED:
        qCDebug(voip) << "GstWebRTCICEConnectionState -> Completed";
        break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_FAILED:
        qCDebug(voip) << "GstWebRTCICEConnectionState -> Failed";
        instance->setState(CallSession::ICEFAILED);
        break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_DISCONNECTED:
        qCDebug(voip) << "GstWebRTCICEConnectionState -> Disconnected";
        break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_CLOSED:
        qCDebug(voip) << "GstWebRTCICEConnectionState -> Closed";
        break;
    default:
        break;
    }
}

GstElement *newAudioSinkChain(GstElement *pipe)
{
    qCDebug(voip) << "New Audio Sink Chain";
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

void sendKeyFrameRequest()
{
    GstPad *sinkpad = gst_element_get_static_pad(_keyFrameRequestData.decodebin, "sink");
    if (!gst_pad_push_event(sinkpad, gst_event_new_custom(GST_EVENT_CUSTOM_UPSTREAM, gst_structure_new_empty("GstForceKeyUnit")))) {
        qCWarning(voip) << "Keyframe request failed";
    }
    gst_object_unref(sinkpad);
}

void _testPacketLoss(GstPromise *promise, gpointer G_GNUC_UNUSED)
{
    const GstStructure *reply = gst_promise_get_reply(promise);
    gint packetsLost = 0;
    GstStructure *rtpStats;
    if (!gst_structure_get(reply, _keyFrameRequestData.statsField.toLatin1().data(), GST_TYPE_STRUCTURE, &rtpStats, nullptr)) {
        qCDebug(voip) << "get-stats: no field:" << _keyFrameRequestData.statsField;
        gst_promise_unref(promise);
        return;
    }
    gst_structure_get_int(rtpStats, "packets-lost", &packetsLost);
    gst_structure_free(rtpStats);
    gst_promise_unref(promise);
    if (packetsLost > _keyFrameRequestData.packetsLost) {
        qCDebug(voip) << "inbound video lost packet count:" << packetsLost;
        _keyFrameRequestData.packetsLost = packetsLost;
        sendKeyFrameRequest();
    }
}

gboolean testPacketLoss(gpointer G_GNUC_UNUSED)
{
    if (_keyFrameRequestData.pipe) {
        GstElement *webrtc = gst_bin_get_by_name(GST_BIN(_keyFrameRequestData.pipe), "webrtcbin");
        GstPromise *promise = gst_promise_new_with_change_func(_testPacketLoss, nullptr, nullptr);
        g_signal_emit_by_name(webrtc, "get-stats", nullptr, promise);
        gst_object_unref(webrtc);
        return true;
    }
    return false;
}

GstElement *newVideoSinkChain(GstElement *pipe, QQuickItem *quickItem)
{
    Q_ASSERT(quickItem);
    qCDebug(voip) << "Creating Video Sink Chain";
    // use compositor for now; acceleration needs investigation
    GstElement *queue = gst_element_factory_make("queue", nullptr);
    GstElement *compositor = gst_element_factory_make("compositor", "compositor");
    GstElement *glupload = gst_element_factory_make("glupload", nullptr);
    GstElement *glcolorconvert = gst_element_factory_make("glcolorconvert", nullptr);
    GstElement *qmlglsink = gst_element_factory_make("qmlglsink", nullptr);
    GstElement *glsinkbin = gst_element_factory_make("glsinkbin", nullptr);
    g_object_set(qmlglsink, "widget", quickItem, nullptr);
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

void linkNewPad(GstElement *decodebin, GstPad *newpad, gpointer user_data)
{
    qCDebug(voip) << "Linking New Pad";
    auto instance = static_cast<CallSession *>(user_data);
    Q_ASSERT(instance);
    GstPad *sinkpad = gst_element_get_static_pad(decodebin, "sink");
    GstCaps *sinkcaps = gst_pad_get_current_caps(sinkpad);
    const GstStructure *structure = gst_caps_get_structure(sinkcaps, 0);

    gchar *mediaType = nullptr;
    guint ssrc = 0;
    gst_structure_get(structure, "media", G_TYPE_STRING, &mediaType, "ssrc", G_TYPE_UINT, &ssrc, nullptr);
    gst_caps_unref(sinkcaps);
    gst_object_unref(sinkpad);

    GstElement *queue = nullptr;
    if (!strcmp(mediaType, "audio")) {
        qCDebug(voip) << "Receiving audio stream";
        //_haveAudioStream = true;
        queue = newAudioSinkChain(instance->m_pipe);
    } else if (!strcmp(mediaType, "video")) {
        qCDebug(voip) << "Receiving video stream";
        instance->setIsReceivingVideo(true);
        queue = newVideoSinkChain(instance->m_pipe, CallManager::instance().m_item);
        _keyFrameRequestData.statsField = QStringLiteral("rtp-inbound-stream-stats_") + QString::number(ssrc);
    } else {
        g_free(mediaType);
        qCWarning(voip) << "Unknown pad type:" << GST_PAD_NAME(newpad);
        return;
    }
    Q_ASSERT(queue);
    GstPad *queuepad = gst_element_get_static_pad(queue, "sink");
    if (queuepad) {
        if (GST_PAD_LINK_FAILED(gst_pad_link(newpad, queuepad))) {
            qCCritical(voip) << "Unable to link new pad";
            // TODO: Error handling
        } else {
            // if (instance->calltype() != CallSession::VIDEO || (_haveAudioStream && (_haveVideoStream || session->isRemoteVideoReceiveOnly()))) {
            instance->setState(CallSession::CONNECTED);
            // if (_haveVideoStream) {
            //     _keyFrameRequestData.pipe = pipe;
            //     _keyFrameRequestData.decodebin = decodebin;
            //     _keyFrameRequestData.timerid = g_timeout_add_seconds(3, testPacketLoss, nullptr);
            // }
            //                 if (session->isRemoteVideoReceiveOnly()) {
            //                     addLocalVideo(pipe);
            //                 }
            //}
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

void addDecodeBin(GstElement *webrtc G_GNUC_UNUSED, GstPad *newpad, gpointer user_data)
{
    if (GST_PAD_DIRECTION(newpad) != GST_PAD_SRC) {
        return;
    }

    auto instance = static_cast<CallSession *>(user_data);
    Q_ASSERT(instance);

    qCDebug(voip) << "Receiving incoming stream";
    GstElement *decodebin = gst_element_factory_make("decodebin", nullptr);
    // Investigate hardware, see nheko source
    g_object_set(decodebin, "force-sw-decoders", TRUE, nullptr);
    g_signal_connect(decodebin, "pad-added", G_CALLBACK(linkNewPad), instance);
    g_signal_connect(decodebin, "element-added", G_CALLBACK(setWaitForKeyFrame), nullptr);
    gst_bin_add(GST_BIN(instance->m_pipe), decodebin);
    gst_element_sync_state_with_parent(decodebin);
    GstPad *sinkpad = gst_element_get_static_pad(decodebin, "sink");
    if (GST_PAD_LINK_FAILED(gst_pad_link(newpad, sinkpad))) {
        // TODO: Error handling
        qCWarning(voip) << "Session: Unable to link decodebin";
    }
    gst_object_unref(sinkpad);
}

void iceGatheringStateChanged(GstElement *webrtc, GParamSpec *pspec G_GNUC_UNUSED, gpointer user_data)
{
    auto instance = static_cast<CallSession *>(user_data);
    Q_ASSERT(instance);

    GstWebRTCICEGatheringState newState;
    g_object_get(webrtc, "ice-gathering-state", &newState, nullptr);
    if (newState == GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE) {
        qCDebug(voip) << "GstWebRTCICEGatheringState -> Complete";
        if (instance->m_isOffering) {
            Q_EMIT instance->offerCreated(instance->m_localSdp, instance->m_localCandidates);
            instance->setState(CallSession::OFFERSENT);
        } else {
            Q_EMIT instance->answerCreated(instance->m_localSdp, instance->m_localCandidates);
            instance->setState(CallSession::ANSWERSENT);
        }
    }
}

gboolean newBusMessage(GstBus *bus G_GNUC_UNUSED, GstMessage *msg, gpointer user_data)
{
    CallSession *instance = static_cast<CallSession *>(user_data);
    Q_ASSERT(instance);
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
        qCDebug(voip) << "End of stream";
        // TODO: Error handling
        instance->end();
        break;
    case GST_MESSAGE_ERROR:
        GError *error;
        gchar *debug;
        gst_message_parse_error(msg, &error, &debug);
        qCWarning(voip) << "Error from element:" << GST_OBJECT_NAME(msg->src) << error->message;
        // TODO: Error handling
        g_clear_error(&error);
        g_free(debug);
        instance->end();
        break;
    default:
        break;
    }
    return TRUE;
}

CallSession::CallSession(QObject *parent)
    : QObject(parent)
{
}

void CallSession::acceptAnswer(const QString &sdp, const QVector<Candidate> &candidates)
{
    qCDebug(voip) << "Accepting Answer";
    if (m_state != CallSession::OFFERSENT) {
        return;
    }

    GstWebRTCSessionDescription *answer = parseSDP(sdp, GST_WEBRTC_SDP_TYPE_ANSWER);
    if (!answer) {
        end();
        return;
    }

    acceptCandidates(candidates);

    int unused;
    //     if (!getMediaAttributes(answer->sdp, "video", "vp8", unused, m_isRemoteVideoReceiveOnly, m_isRemoteVideoSendOnly)) {
    //         m_isRemoteVideoReceiveOnly = true;
    //     } // TODO
    g_signal_emit_by_name(m_webrtc, "set-remote-description", answer, nullptr);
    return;
}

void CallSession::acceptOffer(bool sendVideo, const QString &sdp, const QVector<Candidate> remoteCandidates)
{
    Q_ASSERT(!sdp.isEmpty());
    Q_ASSERT(!remoteCandidates.isEmpty());
    qCDebug(voip) << "Session: Accepting offer";
    if (m_state != CallSession::DISCONNECTED) {
        return;
    }
    m_isOffering = false;

    GstWebRTCSessionDescription *offer = parseSDP(sdp, GST_WEBRTC_SDP_TYPE_OFFER);
    if (!offer) {
        qCritical() << "Session: Offer is not an offer";
        return;
    }

    int opusPayloadType;
    bool receiveOnly;
    bool sendOnly;
    if (getMediaAttributes(offer->sdp, "audio", "opus", opusPayloadType, receiveOnly, sendOnly)) {
        if (opusPayloadType == -1) {
            qCritical() << "Session: No OPUS in offer";
            gst_webrtc_session_description_free(offer);
            return;
        }
    } else {
        qCritical() << "Session: No audio in offer";
        gst_webrtc_session_description_free(offer);
        return;
    }

    int vp8PayloadType;
    // bool isVideo = getMediaAttributes(offer->sdp, "video", "vp8", vp8PayloadType, m_isRemoteVideoReceiveOnly, m_isRemoteVideoSendOnly); // TODO
    //     if (isVideo && vp8PayloadType == -1) {
    //         qCritical() << "Session: No VP8 in offer";
    //         gst_webrtc_session_description_free(offer);
    //         return;
    //     }
    if (!startPipeline(sendVideo)) {
        gst_webrtc_session_description_free(offer);
        qCritical() << "Failed to start pipeline";
        return;
    }
    QThread::msleep(1000); // ?

    acceptCandidates(remoteCandidates);

    GstPromise *promise = gst_promise_new_with_change_func(createAnswer, this, nullptr);
    g_signal_emit_by_name(m_webrtc, "set-remote-description", offer, promise);
    gst_webrtc_session_description_free(offer);
}

void CallSession::createCall(bool sendVideo)
{
    qCDebug(voip) << "Creating call";
    m_isOffering = true;
    startPipeline(sendVideo);
}

bool CallSession::startPipeline(bool sendVideo)
{
    qDebug() << "Session: Starting Pipeline";
    if (m_state != CallSession::DISCONNECTED) {
        return false;
    }
    m_state = CallSession::INITIATING;
    Q_EMIT stateChanged();

    if (!createPipeline(sendVideo)) {
        qCritical() << "Failed to create pipeline";
        end();
        return false;
    }
    m_webrtc = gst_bin_get_by_name(GST_BIN(m_pipe), "webrtcbin");
    Q_ASSERT(m_webrtc);
    if (false /*TODO: CHECK USE STUN*/) {
        qDebug() << "Session: Setting STUN server:" << STUN_SERVER;
        g_object_set(m_webrtc, "stun-server", STUN_SERVER, nullptr);
    }

    for (const auto &uri : m_turnServers) {
        qDebug() << "Session: Setting turn server:" << uri;
        gboolean udata;
        g_signal_emit_by_name(m_webrtc, "add-turn-server", uri.toLatin1().data(), (gpointer)(&udata));
    }

    if (m_turnServers.empty()) {
        qDebug() << "Session: No TURN servers provided";
    }

    if (m_isOffering) {
        qDebug() << "offering";
        g_signal_connect(m_webrtc, "on-negotiation-needed", G_CALLBACK(::createOffer), this);
    }

    g_signal_connect(m_webrtc, "on-ice-candidate", G_CALLBACK(addLocalICECandidate), this);
    g_signal_connect(m_webrtc, "notify::ice-connection-state", G_CALLBACK(iceConnectionStateChanged), this);

    gst_element_set_state(m_pipe, GST_STATE_READY);
    g_signal_connect(m_webrtc, "pad-added", G_CALLBACK(addDecodeBin), this);

    g_signal_connect(m_webrtc, "notify::ice-gathering-state", G_CALLBACK(iceGatheringStateChanged), this);
    gst_object_unref(m_webrtc);

    qDebug() << "Random debug statement #1";

    GstStateChangeReturn ret = gst_element_set_state(m_pipe, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        // TODO: Error handling - unable to start pipeline
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
    qDebug() << "Ending Call";
    if (m_pipe) {
        gst_element_set_state(m_pipe, GST_STATE_NULL);
        gst_object_unref(m_pipe);
        m_pipe = nullptr;
        if (m_busWatchId) {
            g_source_remove(m_busWatchId);
            m_busWatchId = 0;
        }
    }
    if (m_state != CallSession::DISCONNECTED) {
        m_state = CallSession::DISCONNECTED;
        Q_EMIT stateChanged();
    }
}

bool CallSession::createPipeline(bool sendVideo)
{
    qCDebug(voip) << "Creating Pipeline";
    GstDevice *device = AudioSources::instance().currentDevice();
    if (!device) {
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

    GstCaps *rtpcaps = gst_caps_new_simple("application/x-rtp",
                                           "media",
                                           G_TYPE_STRING,
                                           "audio",
                                           "encoding-name",
                                           G_TYPE_STRING,
                                           "OPUS",
                                           "payload",
                                           G_TYPE_INT,
                                           OPUS_PAYLOAD_TYPE,
                                           nullptr);
    g_object_set(capsfilter, "caps", rtpcaps, nullptr);
    gst_caps_unref(rtpcaps);

    GstElement *webrtcbin = gst_element_factory_make("webrtcbin", "webrtcbin");
    g_object_set(webrtcbin, "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE, nullptr);

    m_pipe = gst_pipeline_new(nullptr);
    gst_bin_add_many(GST_BIN(m_pipe), source, volume, convert, resample, queue1, opusenc, rtp, queue2, capsfilter, webrtcbin, nullptr);

    if (!gst_element_link_many(source, volume, convert, resample, queue1, opusenc, rtp, queue2, capsfilter, webrtcbin, nullptr)) {
        qCCritical(voip) << "Failed to link pipeline";
        // TODO propagate errors up and end call
        return false;
    }

    return sendVideo ? addVideoPipeline() : true;
}

bool CallSession::addVideoPipeline()
{
    qWarning() << "Session: Adding Video Pipeline";
    GstElement *camerafilter = nullptr;
    GstElement *videoconvert = gst_element_factory_make("videoconvert", nullptr);
    GstElement *tee = gst_element_factory_make("tee", "videosrctee");
    gst_bin_add_many(GST_BIN(m_pipe), videoconvert, tee, nullptr);
    QPair<int, int> resolution;
    QPair<int, int> frameRate;
    auto device = VideoSources::instance().currentDevice();
    auto deviceCaps = device->caps[VideoSources::instance().capsIndex()];
    setIsSendingVideo(true);
    int width = deviceCaps.width;
    int height = deviceCaps.height;
    int framerate = deviceCaps.framerates.back();
    if (!device) {
        return false;
    }
    GstElement *camera = gst_device_create_element(device->device, nullptr);
    GstCaps *caps =
        gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, "framerate", GST_TYPE_FRACTION, framerate, 1, nullptr);
    camerafilter = gst_element_factory_make("capsfilter", "camerafilter");
    g_object_set(camerafilter, "caps", caps, nullptr);
    gst_caps_unref(caps);

    gst_bin_add_many(GST_BIN(m_pipe), camera, camerafilter, nullptr);
    if (!gst_element_link_many(camera, videoconvert, camerafilter, nullptr)) {
        qCWarning(voip) << "Failed to link camera elements";
        // TODO: Error handling
        return false;
    }
    if (!gst_element_link(camerafilter, tee)) {
        qCWarning(voip) << "Failed to link camerafilter -> tee";
        // TODO: Error handling
        return false;
    }

    GstElement *queue = gst_element_factory_make("queue", nullptr);
    GstElement *vp8enc = gst_element_factory_make("vp8enc", nullptr);
    g_object_set(vp8enc, "deadline", 1, nullptr);
    g_object_set(vp8enc, "error-resilient", 1, nullptr);
    GstElement *rtpvp8pay = gst_element_factory_make("rtpvp8pay", nullptr);
    GstElement *rtpqueue = gst_element_factory_make("queue", nullptr);
    GstElement *rtpcapsfilter = gst_element_factory_make("capsfilter", nullptr);
    GstCaps *rtpcaps = gst_caps_new_simple("application/x-rtp",
                                           "media",
                                           G_TYPE_STRING,
                                           "video",
                                           "encoding-name",
                                           G_TYPE_STRING,
                                           "VP8",
                                           "payload",
                                           G_TYPE_INT,
                                           VP8_PAYLOAD_TYPE,
                                           nullptr);
    g_object_set(rtpcapsfilter, "caps", rtpcaps, nullptr);
    gst_caps_unref(rtpcaps);

    gst_bin_add_many(GST_BIN(m_pipe), queue, vp8enc, rtpvp8pay, rtpqueue, rtpcapsfilter, nullptr);

    GstElement *webrtcbin = gst_bin_get_by_name(GST_BIN(m_pipe), "webrtcbin");
    if (!gst_element_link_many(tee, queue, vp8enc, rtpvp8pay, rtpqueue, rtpcapsfilter, webrtcbin, nullptr)) {
        qCCritical(voip) << "WebRTC: failed to link rtp video elements";
        gst_object_unref(webrtcbin);
        return false;
    }

    gst_object_unref(webrtcbin);
    return true;
}

void CallSession::setTurnServers(QStringList servers)
{
    qDebug() << "Session: Setting Turn Servers";
    m_turnServers = servers;
}

QQuickItem *CallSession::getVideoItem() const
{
    return m_videoItem;
}

void CallSession::acceptCandidates(const QVector<Candidate> &candidates)
{
    Q_ASSERT(!candidates.isEmpty());
    Q_ASSERT(m_webrtc);
    qDebug() << "Session: Accepting ICE Candidates";
    for (const auto &c : candidates) {
        qDebug() << "Remote candidate:" << c.candidate << c.sdpMLineIndex;
        g_signal_emit_by_name(m_webrtc, "add-ice-candidate", c.sdpMLineIndex, c.candidate.toLatin1().data());
    }
}

bool CallSession::havePlugins(bool video) const
{
    GstRegistry *registry = gst_registry_get();
    if (video) {
        const QVector<const char *> videoPlugins = {"compositor", "opengl", "qmlgl", "rtp", "videoconvert", "vpx"};
        for (auto i = 0; i < videoPlugins.size(); i++) {
            auto plugin = gst_registry_find_plugin(registry, videoPlugins[i]);
            if (!plugin) {
                qCCritical(voip) << "Missing GStreamer plugin:" << videoPlugins[i];
                return false;
            }
            gst_object_unref(plugin);
        }
    }

    const QVector<const char *> audioPlugins =
        {"audioconvert", "audioresample", "autodetect", "dtls", "nice", "opus", "playback", "rtpmanager", "srtp", "volume", "webrtc"};
    for (auto i = 0; i < audioPlugins.size(); i++) {
        auto plugin = gst_registry_find_plugin(registry, audioPlugins[i]);
        if (!plugin) {
            qCCritical(voip) << "Missing GStreamer plugin:" << audioPlugins[i];
            return false;
        }
        gst_object_unref(plugin);
    }

    qCInfo(voip) << "GStreamer: All plugins installed";
    return true;
}

void CallSession::setMuted(bool muted)
{
    const auto srclevel = gst_bin_get_by_name(GST_BIN(m_pipe), "srclevel");
    g_object_set(srclevel, "mute", muted, nullptr);
    gst_object_unref(srclevel);
    Q_EMIT mutedChanged();
}

bool CallSession::muted() const
{
    if (m_state < CallSession::CONNECTING) {
        return false;
    }
    if (!m_pipe) {
        return false;
    }
    const auto srclevel = gst_bin_get_by_name(GST_BIN(m_pipe), "srclevel");
    bool muted;
    if (!srclevel) {
        return false;
    }
    g_object_get(srclevel, "mute", &muted, nullptr);
    // gst_object_unref(srclevel); //TODO why does this crash?
    return muted;
}

void CallSession::setIsSendingVideo(bool isSendingVideo)
{
    m_isSendingVideo = isSendingVideo;
    Q_EMIT isSendingVideoChanged();
}

bool CallSession::isSendingVideo() const
{
    return m_isSendingVideo;
}

CallSession *CallSession::acceptCall(bool sendVideo, const QString &sdp, const QVector<Candidate> &candidates, const QStringList &turnUris, QObject *parent)
{
    auto instance = new CallSession(parent);
    instance->setTurnServers(turnUris);
    instance->acceptOffer(sendVideo, sdp, candidates);
    return instance;
}

CallSession *CallSession::startCall(bool sendVideo, const QStringList &turnUris, QObject *parent)
{
    auto instance = new CallSession(parent);

    instance->setTurnServers(turnUris);
    instance->createCall(sendVideo);
    return instance;
}

CallSession::State CallSession::state() const
{
    return m_state;
}

void CallSession::setState(CallSession::State state)
{
    qCDebug(voip) << "Setting state" << state;
    m_state = state;
    Q_EMIT stateChanged();
}

bool CallSession::isReceivingVideo() const
{
    return m_isReceivingVideo;
}

void CallSession::setIsReceivingVideo(bool isReceivingVideo)
{
    m_isReceivingVideo = isReceivingVideo;
    Q_EMIT isReceivingVideoChanged();
}

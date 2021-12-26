// SPDX-FileCopyrightText: 2018-2020 Black Hat <bhat@encom.eu.org>
// SPDX-License-Identifier: GPL-3.0-only

#include "neochatroom.h"

#include <cmark.h>

#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QMimeDatabase>
#include <QTextDocument>
#include <functional>

#include <qcoro/qcorosignal.h>
#include <qcoro/task.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include "connection.h"
#include "csapi/account-data.h"
#include "csapi/content-repo.h"
#include "csapi/leaving.h"
#include "csapi/redaction.h"
#include "csapi/room_state.h"
#include "csapi/rooms.h"
#include "csapi/typing.h"
#include "events/accountdataevents.h"
#include "events/reactionevent.h"
#include "events/roomcanonicalaliasevent.h"
#include "events/roommessageevent.h"
#include "events/roompowerlevelsevent.h"
#include "events/typingevent.h"
#include "jobs/downloadfilejob.h"
#include "neochatconfig.h"
#include "notificationsmanager.h"
#include "stickerevent.h"
#include "user.h"
#include "utils.h"

#include <KLocalizedString>

NeoChatRoom::NeoChatRoom(Connection *connection, QString roomId, JoinState joinState)
    : Room(connection, std::move(roomId), joinState)
{
    connect(this, &NeoChatRoom::notificationCountChanged, this, &NeoChatRoom::countChanged);
    connect(this, &NeoChatRoom::highlightCountChanged, this, &NeoChatRoom::countChanged);
    connect(this, &Room::fileTransferCompleted, this, [this] {
        setFileUploadingProgress(0);
        setHasFileUploading(false);
    });

    connect(this, &Room::aboutToAddHistoricalMessages, this, &NeoChatRoom::readMarkerLoadedChanged);

    connect(this, &Quotient::Room::eventsHistoryJobChanged, this, &NeoChatRoom::lastActiveTimeChanged);

    connect(this, &Room::joinStateChanged, this, [this](JoinState oldState, JoinState newState) {
        if (oldState == JoinState::Invite && newState != JoinState::Invite) {
            Q_EMIT isInviteChanged();
        }
    });
    connect(this, &Room::displaynameChanged, this, &NeoChatRoom::displayNameChanged);

    connectSingleShot(this, &Room::baseStateLoaded, this, [this]() {
        if (this->joinState() != JoinState::Invite) {
            return;
        }
        const QString senderId = getCurrentState<RoomMemberEvent>(localUser()->id())->senderId();
        QImage avatar_image;
        if (!user(senderId)->avatarUrl(this).isEmpty()) {
            avatar_image = user(senderId)->avatar(128, this);
        } else {
            qWarning() << "using this room's avatar";
            avatar_image = avatar(128);
        }
        NotificationsManager::instance().postInviteNotification(this, htmlSafeDisplayName(), htmlSafeMemberName(senderId), avatar_image);
    });
}

#ifdef QUOTIENT_07
QSize getDimension(GstDiscovererStreamInfo *info)
{
    if (!info) {
        return QSize();
    }

    if (GST_IS_DISCOVERER_VIDEO_INFO(info)) {
        GstDiscovererVideoInfo *vinfo = (GstDiscovererVideoInfo *)info;
        // It's a video \o/

        int height = gst_discoverer_video_info_get_height(vinfo);
        int width = gst_discoverer_video_info_get_width(vinfo);
        return QSize(height, width);
    }

    GstDiscovererStreamInfo *next = gst_discoverer_stream_info_get_next(info);
    if (next) {
        auto dimension = getDimension(next);
        gst_discoverer_stream_info_unref(next);
        if (dimension.isValid()) {
            return dimension;
        }
    } else if (GST_IS_DISCOVERER_CONTAINER_INFO(info)) {
        GList *streams = gst_discoverer_container_info_get_streams(GST_DISCOVERER_CONTAINER_INFO(info));

        for (GList *tmp = streams; tmp; tmp = tmp->next) {
            GstDiscovererStreamInfo *tmpinf = (GstDiscovererStreamInfo *)tmp->data;
            auto dimension = getDimension(tmpinf);
            gst_discoverer_stream_info_list_free(streams);
            if (dimension.isValid()) {
                return dimension;
            }
        }
    }
    return QSize();
}

QSize getDimension(const QString &localUrl)
{
    gst_init(0, NULL);
    GError *err = nullptr;
    GstDiscoverer *discoverer = gst_discoverer_new(5 * GST_SECOND, &err);
    if (!discoverer) {
        g_print("Error creating discoverer instance: %s\n", err->message);
        g_error_free(err);
        return QSize();
    }
    const char *url = g_filename_to_uri(localUrl.toStdString().c_str(), NULL, &err);
    if (err) {
        g_warning("Couldn't convert filename to URI: %s\n", err->message);
        g_error_free(err);
        return QSize();
    }

    GstDiscovererInfo *info = gst_discoverer_discover_uri(discoverer, url, &err);
    if (!info) {
        g_print("Error fetching video info: %s\n", err->message);
        g_error_free(err);
        return QSize();
    }
    const gchar *uri = gst_discoverer_info_get_uri(info);

    GstDiscovererResult result = gst_discoverer_info_get_result(info);
    switch (result) {
    case GST_DISCOVERER_URI_INVALID:
        g_print("Invalid URI '%s'\n", uri);
        break;
    case GST_DISCOVERER_ERROR:
        g_print("Discoverer error: %s\n", err->message);
        break;
    case GST_DISCOVERER_TIMEOUT:
        g_print("Timeout\n");
        break;
    case GST_DISCOVERER_BUSY:
        g_print("Busy\n");
        break;
    case GST_DISCOVERER_MISSING_PLUGINS: {
        const GstStructure *s;
        gchar *str;

        s = gst_discoverer_info_get_misc(info);
        str = gst_structure_to_string(s);

        g_print("Missing plugins: %s\n", str);
        g_free(str);
        break;
    }
    case GST_DISCOVERER_OK:
        g_print("Discovered '%s'\n", uri);
        break;
    }
    if (result != GST_DISCOVERER_OK) {
        return QSize();
    }

    GstDiscovererStreamInfo *sinfo = gst_discoverer_info_get_stream_info(info);
    if (!info) {
        return QSize();
    }
    auto dimension = getDimension(sinfo);
    gst_discoverer_stream_info_unref(sinfo);
    g_object_unref(discoverer);
    return dimension;
}

Quotient::EventContent::TypedBase *contentFromFile(const QFileInfo &file)
{
    auto filePath = file.absoluteFilePath();
    auto localUrl = QUrl::fromLocalFile(filePath);
    auto mimeType = QMimeDatabase().mimeTypeForFile(file);
    auto mimeTypeName = mimeType.name();
    if (mimeTypeName.startsWith("image/"))
        return new Quotient::EventContent::ImageContent(localUrl, file.size(), mimeType, QImageReader(filePath).size(), none, file.fileName());

    // duration can only be obtained asynchronously and can only be reliably
    // done by starting to play the file. Left for a future implementation.
    if (mimeTypeName.startsWith("video/")) {
        auto dimension = getDimension(filePath);
        // TODO pass duration to Quotient too
        return new Quotient::EventContent::VideoContent(localUrl, file.size(), mimeType, dimension, none, file.fileName());
    }

    if (mimeTypeName.startsWith("audio/"))
        return new Quotient::EventContent::AudioContent(localUrl, file.size(), mimeType, none, file.fileName());
    return new Quotient::EventContent::FileContent(localUrl, file.size(), mimeType, none, file.fileName());
}
#endif

void NeoChatRoom::uploadFile(const QUrl &url, const QString &body)
{
    if (url.isEmpty()) {
        return;
    }

#ifdef QUOTIENT_07
    const QFileInfo localFile{url.toLocalFile()};
    const QString txnId = postFile(body.isEmpty() ? url.fileName() : body, contentFromFile(localFile));
#else
    const QString txnId = postFile(body.isEmpty() ? url.fileName() : body, url, false);
#endif

    setHasFileUploading(true);
    connect(this, &Room::fileTransferCompleted, [this, txnId](const QString &id, const QUrl & /*localFile*/, const QUrl & /*mxcUrl*/) {
        if (id == txnId) {
            setFileUploadingProgress(0);
            setHasFileUploading(false);
        }
    });
    connect(this, &Room::fileTransferFailed, [this, txnId](const QString &id, const QString & /*error*/) {
        if (id == txnId) {
            setFileUploadingProgress(0);
            setHasFileUploading(false);
        }
    });
    connect(this, &Room::fileTransferProgress, [this, txnId](const QString &id, qint64 progress, qint64 total) {
        if (id == txnId) {
            setFileUploadingProgress(int(float(progress) / float(total) * 100));
        }
    });
}

void NeoChatRoom::acceptInvitation()
{
    connection()->joinRoom(id());
}

void NeoChatRoom::forget()
{
    connection()->forgetRoom(id());
}

QVariantList NeoChatRoom::getUsersTyping() const
{
    auto users = usersTyping();
    users.removeAll(localUser());
    QVariantList userVariants;
    for (User *user : users) {
        userVariants.append(QVariantMap{
            {"id", user->id()},
            {"avatarMediaId", user->avatarMediaId(this)},
            {"displayName", user->displayname(this)},
            {"display", user->name()},
        });
    }
    return userVariants;
}

void NeoChatRoom::sendTypingNotification(bool isTyping)
{
    connection()->callApi<SetTypingJob>(BackgroundRequest, localUser()->id(), id(), isTyping, 10000);
}

const RoomMessageEvent *NeoChatRoom::lastEvent(bool ignoreStateEvent) const
{
    for (auto timelineItem = messageEvents().rbegin(); timelineItem < messageEvents().rend(); timelineItem++) {
        const RoomEvent *event = timelineItem->get();

        if (is<RedactionEvent>(*event) || is<ReactionEvent>(*event)) {
            continue;
        }
        if (event->isRedacted()) {
            continue;
        }

        if (event->isStateEvent()
            && (ignoreStateEvent || !NeoChatConfig::self()->showLeaveJoinEvent() || static_cast<const StateEventBase &>(*event).repeatsState())) {
            continue;
        }

        if (auto roomEvent = eventCast<const RoomMessageEvent>(event)) {
            if (!roomEvent->replacedEvent().isEmpty() && roomEvent->replacedEvent() != roomEvent->id()) {
                continue;
            }
        }

        if (connection()->isIgnored(user(event->senderId()))) {
            continue;
        }

        if (auto lastEvent = eventCast<const RoomMessageEvent>(event)) {
            return lastEvent;
        }
    }
    return nullptr;
}

bool NeoChatRoom::lastEventIsSpoiler() const
{
    if (auto event = lastEvent()) {
        if (auto e = eventCast<const RoomMessageEvent>(event)) {
            if (e->hasTextContent() && e->content() && e->mimeType().name() == "text/html") {
                auto htmlBody = static_cast<const Quotient::EventContent::TextContent *>(e->content())->body;
                return htmlBody.contains("data-mx-spoiler");
            }
        }
    }
    return false;
}

QString NeoChatRoom::lastEventToString() const
{
    if (auto event = lastEvent()) {
        return htmlSafeMemberName(event->senderId()) + (event->isStateEvent() ? " " : ": ") + eventToString(*event);
    }
    return QLatin1String("");
}

bool NeoChatRoom::isEventHighlighted(const RoomEvent *e) const
{
    return highlights.contains(e);
}

void NeoChatRoom::checkForHighlights(const Quotient::TimelineItem &ti)
{
    auto localUserId = localUser()->id();
    if (ti->senderId() == localUserId) {
        return;
    }
    if (auto *e = ti.viewAs<RoomMessageEvent>()) {
        const auto &text = e->plainBody();
        if (text.contains(localUserId) || text.contains(roomMembername(localUserId))) {
            highlights.insert(e);
        }
    }
}

void NeoChatRoom::onAddNewTimelineEvents(timeline_iter_t from)
{
    std::for_each(from, messageEvents().cend(), [this](const TimelineItem &ti) {
        checkForHighlights(ti);
    });
}

void NeoChatRoom::onAddHistoricalTimelineEvents(rev_iter_t from)
{
    std::for_each(from, messageEvents().crend(), [this](const TimelineItem &ti) {
        checkForHighlights(ti);
    });
}

void NeoChatRoom::onRedaction(const RoomEvent &prevEvent, const RoomEvent & /*after*/)
{
    if (const auto &e = eventCast<const ReactionEvent>(&prevEvent)) {
        if (auto relatedEventId = e->relation().eventId; !relatedEventId.isEmpty()) {
            Q_EMIT updatedEvent(relatedEventId);
        }
    }
}

void NeoChatRoom::countChanged()
{
    if (displayed() && !hasUnreadMessages()) {
        resetNotificationCount();
        resetHighlightCount();
    }
}

QDateTime NeoChatRoom::lastActiveTime()
{
    if (timelineSize() == 0) {
        return QDateTime();
    }

    if (auto event = lastEvent(true)) {
        return event->originTimestamp();
    }

    // no message found, take last event
    return messageEvents().rbegin()->get()->originTimestamp();
}

int NeoChatRoom::savedTopVisibleIndex() const
{
    return firstDisplayedMarker() == historyEdge() ? 0 : int(firstDisplayedMarker() - messageEvents().rbegin());
}

int NeoChatRoom::savedBottomVisibleIndex() const
{
    return lastDisplayedMarker() == historyEdge() ? 0 : int(lastDisplayedMarker() - messageEvents().rbegin());
}

void NeoChatRoom::saveViewport(int topIndex, int bottomIndex)
{
    if (topIndex == -1 || bottomIndex == -1 || (bottomIndex == savedBottomVisibleIndex() && (bottomIndex == 0 || topIndex == savedTopVisibleIndex()))) {
        return;
    }
    if (bottomIndex == 0) {
        setFirstDisplayedEventId({});
        setLastDisplayedEventId({});
        return;
    }
    setFirstDisplayedEvent(maxTimelineIndex() - topIndex);
    setLastDisplayedEvent(maxTimelineIndex() - bottomIndex);
}

QVariantList NeoChatRoom::getUsers(const QString &keyword, int limit) const
{
    const auto userList = users();
    QVariantList matchedList;
    int count = 0;
    for (const auto u : userList) {
        if (u->displayname(this).contains(keyword, Qt::CaseInsensitive)) {
            NeoChatUser user(u->id(), u->connection());
            QVariantMap userVariant{{QStringLiteral("id"), user.id()},
                                    {QStringLiteral("displayName"), user.displayname(this)},
                                    {QStringLiteral("avatarMediaId"), user.avatarMediaId(this)},
                                    {QStringLiteral("color"), user.color()}};

            matchedList.append(QVariant::fromValue(userVariant));
            count++;
            if (count == limit) { // -1 is infinite
                break;
            }
        }
    }

    return matchedList;
}

QVariantMap NeoChatRoom::getUser(const QString &userID) const
{
    NeoChatUser user(userID, connection());
    return QVariantMap{{QStringLiteral("id"), user.id()},
                       {QStringLiteral("displayName"), user.displayname(this)},
                       {QStringLiteral("avatarMediaId"), user.avatarMediaId(this)},
                       {QStringLiteral("color"), user.color()}};
}

QUrl NeoChatRoom::urlToMxcUrl(const QUrl &mxcUrl)
{
    return DownloadFileJob::makeRequestUrl(connection()->homeserver(), mxcUrl);
}

QString NeoChatRoom::avatarMediaId() const
{
    if (const auto avatar = Room::avatarMediaId(); !avatar.isEmpty()) {
        return avatar;
    }

    // Use the first (excluding self) user's avatar for direct chats
    const auto dcUsers = directChatUsers();
    for (const auto u : dcUsers) {
        if (u != localUser()) {
            return u->avatarMediaId(this);
        }
    }

    return {};
}

QString NeoChatRoom::eventToString(const RoomEvent &evt, Qt::TextFormat format, bool removeReply) const
{
    const bool prettyPrint = (format == Qt::RichText);

    using namespace Quotient;
#ifdef QUOTIENT_07
    return switchOnType(
#else
    return visit(
#endif
        evt,
        [this, prettyPrint, removeReply](const RoomMessageEvent &e) {
            using namespace MessageEventContent;

            // 1. prettyPrint/HTML
            if (prettyPrint && e.hasTextContent() && e.mimeType().name() != "text/plain") {
                auto htmlBody = static_cast<const TextContent *>(e.content())->body;
                if (removeReply) {
                    htmlBody.remove(utils::removeRichReplyRegex);
                }
                htmlBody.replace(utils::userPillRegExp, R"(<b class="user-pill">\1</b>)");
                htmlBody.replace(utils::strikethroughRegExp, "<s>\\1</s>");

                auto url = connection()->homeserver();
                auto base = url.scheme() + QStringLiteral("://") + url.host() + (url.port() != -1 ? ':' + QString::number(url.port()) : QString());
                htmlBody.replace(utils::mxcImageRegExp, QStringLiteral(R"(<img \1 src="%1/_matrix/media/r0/download/\2/\3" \4 > )").arg(base));

                return htmlBody;
            }

            if (e.hasFileContent()) {
                auto fileCaption = e.content()->fileInfo()->originalName.toHtmlEscaped();
                if (fileCaption.isEmpty()) {
                    fileCaption = prettyPrint ? Quotient::prettyPrint(e.plainBody()) : e.plainBody();
                } else if (e.content()->fileInfo()->originalName != e.plainBody()) {
                    fileCaption = e.plainBody() + " | " + fileCaption;
                }
                return !fileCaption.isEmpty() ? fileCaption : i18n("a file");
            }

            // 2. prettyPrint/text 3. plainText/HTML 4. plainText/text
            QString plainBody;
            if (e.hasTextContent() && e.content() && e.mimeType().name() == "text/plain") { // 2/4
                plainBody = static_cast<const TextContent *>(e.content())->body;
            } else { // 3
                plainBody = e.plainBody();
            }

            if (prettyPrint) {
                if (removeReply) {
                    plainBody.remove(utils::removeReplyRegex);
                }
                return Quotient::prettyPrint(plainBody);
            }
            if (removeReply) {
                return plainBody.remove(utils::removeReplyRegex);
            }
            return plainBody;
        },
        [](const StickerEvent &e) {
            return e.body();
        },
        [this](const RoomMemberEvent &e) {
            // FIXME: Rewind to the name that was at the time of this event
            auto subjectName = this->htmlSafeMemberName(e.userId());
            // The below code assumes senderName output in AuthorRole
            switch (e.membership()) {
            case MembershipType::Invite:
                if (e.repeatsState()) {
                    auto text = i18n("reinvited %1 to the room", subjectName);
                    if (!e.reason().isEmpty()) {
                        text += i18nc("Optional reason for an invitation", ": %1") + e.reason().toHtmlEscaped();
                    }
                    return text;
                }
                Q_FALLTHROUGH();
            case MembershipType::Join: {
                QString text{};
                // Part 1: invites and joins
                if (e.repeatsState()) {
                    text = i18n("joined the room (repeated)");
                } else if (e.changesMembership()) {
                    text = e.membership() == MembershipType::Invite ? i18n("invited %1 to the room", subjectName) : i18n("joined the room");
                }
                if (!text.isEmpty()) {
                    if (!e.reason().isEmpty()) {
                        text += i18n(": %1", e.reason().toHtmlEscaped());
                    }
                    return text;
                }
                // Part 2: profile changes of joined members
                if (e.isRename() && NeoChatConfig::self()->showRename()) {
                    if (e.displayName().isEmpty()) {
                        text = i18nc("their refers to a singular user", "cleared their display name");
                    } else {
                        text = i18nc("their refers to a singular user", "changed their display name to %1", e.displayName().toHtmlEscaped());
                    }
                }
                if (e.isAvatarUpdate() && NeoChatConfig::self()->showAvatarUpdate()) {
                    if (!text.isEmpty()) {
                        text += i18n(" and ");
                    }
                    if (e.avatarUrl().isEmpty()) {
                        text += i18nc("their refers to a singular user", "cleared their avatar");
#ifdef QUOTIENT_07
                    } else if (!e.prevContent()->avatarUrl) {
#else
                    } else if (e.prevContent()->avatarUrl.isEmpty()) {
#endif
                        text += i18n("set an avatar");
                    } else {
                        text += i18nc("their refers to a singular user", "updated their avatar");
                    }
                }
                return text;
            }
            case MembershipType::Leave:
                if (e.prevContent() && e.prevContent()->membership == MembershipType::Invite) {
                    return (e.senderId() != e.userId()) ? i18n("withdrew %1's invitation", subjectName) : i18n("rejected the invitation");
                }

                if (e.prevContent() && e.prevContent()->membership == MembershipType::Ban) {
                    return (e.senderId() != e.userId()) ? i18n("unbanned %1", subjectName) : i18n("self-unbanned");
                }
                return (e.senderId() != e.userId())
                    ? i18n("has put %1 out of the room: %2", subjectName, e.contentJson()["reason"_ls].toString().toHtmlEscaped())
                    : i18n("left the room");
            case MembershipType::Ban:
                return (e.senderId() != e.userId()) ? i18n("banned %1 from the room: %2", subjectName, e.contentJson()["reason"_ls].toString().toHtmlEscaped())
                                                    : i18n("self-banned from the room");
            case MembershipType::Knock:
                return i18n("requested an invite");
            default:;
            }
            return i18n("made something unknown");
        },
        [](const RoomCanonicalAliasEvent &e) {
            return (e.alias().isEmpty()) ? i18n("cleared the room main alias") : i18n("set the room main alias to: %1", e.alias());
        },
        [](const RoomNameEvent &e) {
            return (e.name().isEmpty()) ? i18n("cleared the room name") : i18n("set the room name to: %1", e.name().toHtmlEscaped());
        },
        [prettyPrint](const RoomTopicEvent &e) {
            return (e.topic().isEmpty()) ? i18n("cleared the topic") : i18n("set the topic to: %1", prettyPrint ? Quotient::prettyPrint(e.topic()) : e.topic());
        },
        [](const RoomAvatarEvent &) {
            return i18n("changed the room avatar");
        },
        [](const EncryptionEvent &) {
            return i18n("activated End-to-End Encryption");
        },
        [](const RoomCreateEvent &e) {
            return e.isUpgrade() ? i18n("upgraded the room to version %1", e.version().isEmpty() ? "1" : e.version().toHtmlEscaped())
                                 : i18n("created the room, version %1", e.version().isEmpty() ? "1" : e.version().toHtmlEscaped());
        },
        [](const RoomPowerLevelsEvent &) {
            return i18nc("'power level' means permission level", "changed the power levels for this room");
        },
        [](const StateEventBase &e) {
            if (e.matrixType() == QLatin1String("m.room.server_acl")) {
                return i18n("changed the server access control lists for this room");
            }
            return e.stateKey().isEmpty() ? i18n("updated %1 state", e.matrixType())
                                          : i18n("updated %1 state for %2", e.matrixType(), e.stateKey().toHtmlEscaped());
        },
        i18n("Unknown event"));
}

void NeoChatRoom::changeAvatar(const QUrl &localFile)
{
    const auto job = connection()->uploadFile(localFile.toLocalFile());
#ifdef QUOTIENT_07
    if (isJobPending(job)) {
#else
    if (isJobRunning(job)) {
#endif
        connect(job, &BaseJob::success, this, [this, job] {
#ifdef QUOTIENT_07
            connection()->callApi<SetRoomStateWithKeyJob>(id(), "m.room.avatar", localUser()->id(), QJsonObject{{"url", job->contentUri().toString()}});
#else
            connection()->callApi<SetRoomStateWithKeyJob>(id(), "m.room.avatar", localUser()->id(), QJsonObject{{"url", job->contentUri()}});
#endif
        });
    }
}

void NeoChatRoom::addLocalAlias(const QString &alias)
{
    auto a = aliases();
    if (a.contains(alias)) {
        return;
    }

    a += alias;

    setLocalAliases(a);
}

void NeoChatRoom::removeLocalAlias(const QString &alias)
{
    auto a = aliases();
    if (!a.contains(alias)) {
        return;
    }

    a.removeAll(alias);

    setLocalAliases(a);
}

QString NeoChatRoom::markdownToHTML(const QString &markdown)
{
    const auto str = markdown.toUtf8();
    char *tmp_buf = cmark_markdown_to_html(str.constData(), str.size(), CMARK_OPT_DEFAULT);

    const std::string html(tmp_buf);

    free(tmp_buf);

    auto result = QString::fromStdString(html).trimmed();

    result.replace("<!-- raw HTML omitted -->", "<br />");
    result.replace(QRegularExpression("(<br />)*$"), "");
    result.replace("<p>", "");
    result.replace("</p>", "");

    return result;
}

QString msgTypeToString(MessageEventType msgType)
{
    switch (msgType) {
    case MessageEventType::Text:
        return "m.text";
    case MessageEventType::File:
        return "m.file";
    case MessageEventType::Audio:
        return "m.audio";
    case MessageEventType::Emote:
        return "m.emote";
    case MessageEventType::Image:
        return "m.image";
    case MessageEventType::Video:
        return "m.video";
    case MessageEventType::Notice:
        return "m.notice";
    case MessageEventType::Location:
        return "m.location";
    default:
        return "m.text";
    }
}

QString NeoChatRoom::preprocessText(const QString &text)
{
    return markdownToHTML(text);
}

void NeoChatRoom::postMessage(const QString &rawText, const QString &text, MessageEventType type, const QString &replyEventId, const QString &relateToEventId)
{
    postHtmlMessage(rawText, text, type, replyEventId, relateToEventId);
}

void NeoChatRoom::postHtmlMessage(const QString &text, const QString &html, MessageEventType type, const QString &replyEventId, const QString &relateToEventId)
{
    bool isRichText = Qt::mightBeRichText(html);
    bool isReply = !replyEventId.isEmpty();
    bool isEdit = !relateToEventId.isEmpty();
    const auto replyIt = findInTimeline(replyEventId);
    if (replyIt == historyEdge()) {
        isReply = false;
    }

    if (isEdit) {
        QJsonObject json{
            {"type", "m.room.message"},
            {"msgtype", msgTypeToString(type)},
            {"body", "* " + text},
            {"format", "org.matrix.custom.html"},
            {"formatted_body", html},
            {"m.new_content", QJsonObject{{"body", text}, {"msgtype", msgTypeToString(type)}, {"format", "org.matrix.custom.html"}, {"formatted_body", html}}},
            {"m.relates_to", QJsonObject{{"rel_type", "m.replace"}, {"event_id", relateToEventId}}}};

        postJson("m.room.message", json);
        return;
    }

    if (isReply) {
        const auto &replyEvt = **replyIt;

        // clang-format off
        QJsonObject json{
          {"msgtype", msgTypeToString(type)},
          {"body", "> <" + replyEvt.senderId() + "> " + eventToString(replyEvt) + "\n\n" + text},
          {"format", "org.matrix.custom.html"},
          {"m.relates_to",
            QJsonObject {
              {"m.in_reply_to",
                QJsonObject {
                  {"event_id", replyEventId}
                }
              }
            }
          },
          {"formatted_body",
            "<mx-reply><blockquote><a href=\"https://matrix.to/#/" +
            id() + "/" +
            replyEventId +
            "\">In reply to</a> <a href=\"https://matrix.to/#/" +
            replyEvt.senderId() + "\">" + replyEvt.senderId() +
            "</a><br>" + eventToString(replyEvt, Qt::RichText) +
            "</blockquote></mx-reply>" + (isRichText ? html : text)
          }
        };
        // clang-format on

        postJson("m.room.message", json);

        return;
    }

    if (isRichText) {
        Room::postHtmlMessage(text, html, type);
    } else {
        Room::postMessage(text, type);
    }
}

void NeoChatRoom::toggleReaction(const QString &eventId, const QString &reaction)
{
    if (eventId.isEmpty() || reaction.isEmpty()) {
        return;
    }

    const auto eventIt = findInTimeline(eventId);
    if (eventIt == historyEdge()) {
        return;
    }

    const auto &evt = **eventIt;

    QStringList redactEventIds; // What if there are multiple reaction events?

    const auto &annotations = relatedEvents(evt, EventRelation::Annotation());
    if (!annotations.isEmpty()) {
        for (const auto &a : annotations) {
            if (auto e = eventCast<const ReactionEvent>(a)) {
                if (e->relation().key != reaction) {
                    continue;
                }

                if (e->senderId() == localUser()->id()) {
                    redactEventIds.push_back(e->id());
                    break;
                }
            }
        }
    }

    if (!redactEventIds.isEmpty()) {
        for (const auto &redactEventId : redactEventIds) {
            redactEvent(redactEventId);
        }
    } else {
        postReaction(eventId, reaction);
    }
}

bool NeoChatRoom::containsUser(const QString &userID) const
{
    auto u = Room::user(userID);

    if (!u) {
        return false;
    }

    return Room::memberJoinState(u) != JoinState::Leave;
}

bool NeoChatRoom::canSendEvent(const QString &eventType) const
{
    auto plEvent = getCurrentState<RoomPowerLevelsEvent>();
    auto pl = plEvent->powerLevelForEvent(eventType);
    auto currentPl = plEvent->powerLevelForUser(localUser()->id());

    return currentPl >= pl;
}

bool NeoChatRoom::canSendState(const QString &eventType) const
{
    auto plEvent = getCurrentState<RoomPowerLevelsEvent>();
    auto pl = plEvent->powerLevelForState(eventType);
    auto currentPl = plEvent->powerLevelForUser(localUser()->id());

    return currentPl >= pl;
}

bool NeoChatRoom::readMarkerLoaded() const
{
    const auto it = findInTimeline(readMarkerEventId());
    return it != historyEdge();
}

bool NeoChatRoom::isInvite() const
{
    return joinState() == JoinState::Invite;
}

bool NeoChatRoom::isUserBanned(const QString &user) const
{
    return getCurrentState<RoomMemberEvent>(user)->membership() == MembershipType::Ban;
}

QString NeoChatRoom::htmlSafeName() const
{
    return name().toHtmlEscaped();
}

QString NeoChatRoom::htmlSafeDisplayName() const
{
    return displayName().toHtmlEscaped();
}

void NeoChatRoom::deleteMessagesByUser(const QString &user)
{
    doDeleteMessagesByUser(user);
}

QString NeoChatRoom::joinRule() const
{
    return getCurrentState<JoinRulesEvent>()->joinRule();
}

QCoro::Task<void> NeoChatRoom::doDeleteMessagesByUser(const QString &user)
{
    QStringList events;
    for (const auto &event : messageEvents()) {
        if (event->senderId() == user && !event->isRedacted() && !event.viewAs<RedactionEvent>() && !event->isStateEvent()) {
            events += event->id();
        }
    }
    for (const auto &e : events) {
        auto job = connection()->callApi<RedactEventJob>(id(), QUrl::toPercentEncoding(e), connection()->generateTxnId());
        co_await qCoro(job, &BaseJob::finished);
        if (job->error() != BaseJob::Success) {
            qWarning() << "Error: \"" << job->error() << "\" while deleting messages. Aborting";
            break;
        }
    }
}

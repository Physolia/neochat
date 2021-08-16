// SPDX-FileCopyrightText: 2020 Carl Schwan <carlschwan@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "stickerevent.h"

using namespace Quotient;

StickerEvent::StickerEvent(const QJsonObject &obj)
    : RoomEvent(typeId(), obj)
    , m_imageContent(EventContent::ImageContent(obj["content"_ls].toObject()))
{
}

StickerEvent::StickerEvent(const QString &body, const Quotient::EventContent::ImageInfo &imageInfo, const QUrl &url)
    : RoomEvent(typeId(),
                {{"type", "m.sticker"},
                 {"content",
                  QJsonObject{
                      {"body", body},
                      {"url", url.toString()},
                  }}})
    , m_imageContent({
          {"body", body},
          {"url", url.toString()},
      })
{
    QJsonObject infoJson;
    imageInfo.fillInfoJson(&infoJson);
    auto content = editJson()["content"].toObject();
    content.insert("info", infoJson);
    editJson().insert("content", content);
}

QString StickerEvent::body() const
{
    return content<QString>("body"_ls);
}

const EventContent::ImageContent &StickerEvent::image() const
{
    return m_imageContent;
}

QUrl StickerEvent::url() const
{
    return m_imageContent.url;
}

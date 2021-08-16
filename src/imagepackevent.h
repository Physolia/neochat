// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: LGPL-2.0-or-later

#pragma once

#include "events/eventcontent.h"
#include "events/stateevent.h"
#include <QVector>

using namespace Quotient;

class ImagePackEventContent : public EventContent::Base
{
public:
    struct Pack {
        Omittable<QString> displayName;
        Omittable<QUrl> avatarUrl;
        Omittable<QStringList> usage;
        Omittable<QString> attribution;
    };

    struct ImagePackImage {
        QString shortcode;
        QUrl url;
        Omittable<QString> body;
        Omittable<Quotient::EventContent::ImageInfo> info;
        Omittable<QStringList> usage;
    };

    Omittable<Pack> pack;
    QVector<ImagePackEventContent::ImagePackImage> images;

    explicit ImagePackEventContent(const QJsonObject &o);

protected:
    void fillJson(QJsonObject *o) const override;
};

class ImagePackEvent : public StateEvent<ImagePackEventContent>
{
    Q_GADGET
public:
    DEFINE_EVENT_TYPEID("im.ponies.room_emotes", ImagePackEvent)

    explicit ImagePackEvent(const QJsonObject &obj)
        : StateEvent(typeId(), obj)
    {}

    QVector<ImagePackEventContent::ImagePackImage> images() const { return content().images; }
    Omittable<ImagePackEventContent::Pack> pack() const { return content().pack; }

};
REGISTER_EVENT_TYPE(ImagePackEvent);

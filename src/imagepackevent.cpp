// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: LGPL-2.0-or-later

#include "imagepackevent.h"
#include <QJsonObject>

ImagePackEventContent::ImagePackEventContent(const QJsonObject &json)
{
    if(json.contains(QStringLiteral("pack"))) {
        pack = ImagePackEventContent::Pack{
            fromJson<Omittable<QString>>(json["pack"].toObject()["display_name"]),
            fromJson<Omittable<QUrl>>(json["pack"].toObject()["avatar_url"]),
            fromJson<Omittable<QStringList>>(json["pack"].toObject()["usage"]),
            fromJson<Omittable<QString>>(json["pack"].toObject()["attribution"]),
        };
    } else {
        pack = none;
    }

    for(const auto &k : json["images"].toObject().keys()) {
        Omittable<EventContent::ImageInfo> info;
        if(k.contains(QStringLiteral("info"))) {
#ifdef QUOTIENT_07
            info = EventContent::ImageInfo(QUrl(json["images"][k]["url"].toString()), json["images"][k].toObject(), none, k);
#else
            info = EventContent::ImageInfo(QUrl(json["images"][k]["url"].toString()), json["images"][k].toObject(), k);
#endif
        } else {
            info = none;
        }
        images += ImagePackImage {
            k,
            fromJson<QUrl>(json["images"][k]["url"]),
            fromJson<Omittable<QString>>(json["images"][k]["body"]),
            info,
            fromJson<Omittable<QStringList>>(json["images"][k]["usage"]),
        };
    }
}

void ImagePackEventContent::fillJson(QJsonObject* o) const {
    // TODO
}

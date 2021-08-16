// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: LGPL-2.0-or-later

#include "stickermodel.h"

#include "neochatroom.h"

#include "imagepackevent.h"

#include <algorithm>

#include <KLocalizedString>

StickerModel::StickerModel(QObject *parent)
    : QAbstractListModel(parent)
{}

void StickerModel::setRoom(NeoChatRoom *room)
{
    if(m_room) {
        disconnect(m_room, &Room::changed, this, nullptr);
    }
    m_room = room;
    Q_EMIT roomChanged();
    connect(m_room, &Room::changed, this, [=](){
        beginResetModel();
        endResetModel();
    });
}

NeoChatRoom *StickerModel::room() const
{
    return m_room;
}

int StickerModel::rowCount(const QModelIndex &index) const
{
    if(!m_room) {
        return 0;
    }
    const auto &images = m_room->getCurrentState<ImagePackEvent>(m_pack)->content().images;
    return std::count_if(images.constBegin(), images.constEnd(), [](const auto &image){
        return !image.usage.has_value() || image.usage->contains("sticker") || image.usage->isEmpty();
    });
}
QVariant StickerModel::data(const QModelIndex &index, int role) const
{
    if(!m_room) {
        return QVariant();
    }
    const auto &images = m_room->getCurrentState<ImagePackEvent>(m_pack)->content().images;
    int _count = 0;
    const auto &image = std::find_if(images.constBegin(), images.constEnd(), [&_count, index](ImagePackEventContent::ImagePackImage const &x) {
        return (!x.usage.has_value() || x.usage->contains("sticker") || x.usage->isEmpty()) && _count++ == index.row();
    });
    if(role == StickerModel::Url) {
        return image->url.toString().remove(0, 6);
    } else if (role == StickerModel::Body) {
        return image->body ? *image->body : i18n("Unnamed Sticker");
    }
    return QVariant("DEADBEEF");
}

QHash<int, QByteArray> StickerModel::roleNames() const
{
    return {
        {StickerModel::Url, "url"},
        {StickerModel::Body, "body"},
    };
}

void StickerModel::postSticker(unsigned int index)
{
    if(!m_room) {
        return;
    }
    const auto &images = m_room->getCurrentState<ImagePackEvent>(m_pack)->content().images;
    int _count = 0;
    const auto &image = std::find_if(images.constBegin(), images.constEnd(), [&_count, index](ImagePackEventContent::ImagePackImage const &x) {
        return (!x.usage.has_value() || x.usage->contains("sticker") || x.usage->isEmpty()) && _count++ == index;
    });
    m_room->postSticker(image->body.has_value() ? *image->body : QString(), image->info.has_value() ? *image->info : Quotient::EventContent::ImageInfo(image->url), image->url);
}

QString StickerModel::pack() const
{
    return m_pack;
}

void StickerModel::setPack(const QString &pack)
{
    m_pack = pack;
    Q_EMIT packChanged();
}

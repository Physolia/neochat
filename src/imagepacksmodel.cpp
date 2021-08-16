// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: LGPL-2.0-or-later

#include "imagepacksmodel.h"
#include "imagepackevent.h"
#include "neochatroom.h"

#include <KLocalizedString>

ImagePacksModel::ImagePacksModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ImagePacksModel::rowCount(const QModelIndex &index) const
{
    auto events = m_room->stateEventsOfType("im.ponies.room_emotes");
    return std::count_if(events.constBegin(), events.constEnd(), [](const auto &_event) {
        const auto event = eventCast<const ImagePackEvent>(_event);
        return !event->pack() || !event->pack()->usage || event->pack()->usage->contains("sticker") || event->pack()->usage->isEmpty();
    });
    // TODO + account
    // TODO + rooms
}

QVariant ImagePacksModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_room->stateEventsOfType("im.ponies.room_emotes").size()) {
        return QVariant();
    }
    const auto pack = eventCast<const ImagePackEvent>(m_room->stateEventsOfType("im.ponies.room_emotes")[index.row()]);
    switch (role) {
    case DisplayNameRole: {
        if (pack->pack() && pack->pack()->displayName) {
            return *pack->pack()->displayName;
        } else {
            return i18n("Unnamed image pack");
        }
    }
    case AvatarUrlRole: {
        if (pack->pack() && pack->pack()->avatarUrl) {
            return (*pack->pack()->avatarUrl).toString().mid(6);
        } else {
            return QVariant();
        }
    }
    case AttributionRole: {
        if (pack->pack() && pack->pack()->attribution) {
            return *pack->pack()->attribution;
        } else {
            return QVariant();
        }
    }
    case IdRole: {
        return pack->stateKey();
    }
    }
    return QVariant();
}

QHash<int, QByteArray> ImagePacksModel::roleNames() const
{
    return {{DisplayNameRole, "displayName"}, {AvatarUrlRole, "avatarUrl"}, {AttributionRole, "attribution"}, {IdRole, "id"}};
}

NeoChatRoom *ImagePacksModel::room() const
{
    return m_room;
}

void ImagePacksModel::setRoom(NeoChatRoom *room)
{
    m_room = room;
    Q_EMIT roomChanged();
}

bool ImagePacksModel::showStickers() const
{
    return m_showStickers;
}

void ImagePacksModel::setShowStickers(bool showStickers)
{
    m_showStickers = showStickers;
    Q_EMIT showStickersChanged();
}

bool ImagePacksModel::showEmoticons() const
{
    return m_showEmoticons;
}

void ImagePacksModel::setShowEmoticons(bool showEmoticons)
{
    m_showEmoticons = showEmoticons;
    Q_EMIT showEmoticonsChanged();
}

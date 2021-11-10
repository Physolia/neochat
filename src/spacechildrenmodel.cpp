// SPDX-FileCopyrightText: 2019-2020 Black Hat <bhat@encom.eu.org>
// SPDX-FileCopyrightText: 2021 Smitty van Bodegom <me@smitop.com>
// SPDX-License-Identifier: GPL-3.0-only

#include "spacechildrenmodel.h"
#include "neochatroom.h"

SpaceChildrenModel::SpaceChildrenModel(QObject *parent, QList<Room *> rooms, Connection *conn)
    : QAbstractListModel(parent)
    , m_rooms(rooms)
    , m_connection(conn)
{
}

QVariant SpaceChildrenModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= m_rooms.count()) {
        qDebug() << "SpaceChildrenModel, something's wrong: index.row() >= "
                    "rooms.count()";
        return {};
    }
    auto room = m_rooms.at(index.row());
    switch (role) {
    case NameRole: {
        auto displayName = room->name();
        if (!displayName.isEmpty()) {
            return displayName;
        }

        displayName = room->canonicalAlias();
        if (!displayName.isEmpty()) {
            return displayName;
        }

        if (!room->aliases().isEmpty()) {
            displayName = room->aliases().front();
        }

        if (!displayName.isEmpty()) {
            return displayName;
        }

        return room->id();
    }
    case AvatarRole: {
        auto avatarUrl = room->avatarUrl();

        if (avatarUrl.isEmpty()) {
            return "";
        }
#ifdef QUOTIENT_07
        return avatarUrl.url().remove(0, 6);
#else
        return avatarUrl.remove(0, 6);
#endif
    }
    case TopicRole:
        return room->topic();
    case RoomIDRole:
        return room->id();
    case AliasRole:
        if (!room->canonicalAlias().isEmpty()) {
            return room->canonicalAlias();
        }
        return {};
    case MemberCountRole:
        return room->totalMemberCount();
    case AllowGuestsRole:
    case WorldReadableRole:
        return false; // TODO
    case IsJoinedRole:
        if (!m_connection) {
            return {};
        }
        return m_connection->room(room->id(), JoinState::Join) != nullptr;
    }

    return {};
}

QHash<int, QByteArray> SpaceChildrenModel::roleNames() const
{
    return {{NameRole, QByteArrayLiteral("name")},
            {AvatarRole, QByteArrayLiteral("avatar")},
            {TopicRole, QByteArrayLiteral("topic")},
            {RoomIDRole, QByteArrayLiteral("roomID")},
            {MemberCountRole, QByteArrayLiteral("memberCount")},
            {AllowGuestsRole, QByteArrayLiteral("allowGuests")},
            {WorldReadableRole, QByteArrayLiteral("worldReadable")},
            {IsJoinedRole, QByteArrayLiteral("isJoined")},
            {AliasRole, QByteArrayLiteral("alias")}};
}

int SpaceChildrenModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_rooms.count();
}

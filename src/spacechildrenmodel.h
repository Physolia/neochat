// SPDX-FileCopyrightText: 2021 Smitty van Bodegom <me@smitop.com>
// SPDX-License-Identifier: GPL-3.0-only

// has same QML API as PublicRoomListModel

#pragma once

#include <QAbstractListModel>
#include <QObject>

#include "connection.h"

using namespace Quotient;

class SpaceChildrenModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum EventRoles {
        NameRole = Qt::UserRole + 1,
        AvatarRole,
        TopicRole,
        RoomIDRole,
        AliasRole,
        MemberCountRole,
        AllowGuestsRole,
        WorldReadableRole,
        IsJoinedRole,
    };

    explicit SpaceChildrenModel(QObject *parent, QList<Room *> rooms, Connection *conn);

    [[nodiscard]] QVariant data(const QModelIndex &index, int role = NameRole) const override;
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    QList<Room *> m_rooms;
    Connection *m_connection = nullptr;
};

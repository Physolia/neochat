// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: LGPL-2.0-or-later

#pragma once

#include <QAbstractListModel>

class NeoChatRoom;

class ImagePacksModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(NeoChatRoom *room READ room WRITE setRoom NOTIFY roomChanged)
    Q_PROPERTY(bool showStickers READ showStickers WRITE setShowStickers NOTIFY showStickersChanged)
    Q_PROPERTY(bool showEmoticons READ showEmoticons WRITE setShowEmoticons NOTIFY showEmoticonsChanged)

public:
    enum Roles {
        DisplayNameRole = Qt::UserRole + 1,
        AvatarUrlRole,
        AttributionRole,
        IdRole,
    };

    ImagePacksModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    NeoChatRoom *room() const;
    void setRoom(NeoChatRoom *room);

    bool showStickers() const;
    void setShowStickers(bool showStickers);

    bool showEmoticons() const;
    void setShowEmoticons(bool showEmoticons);

Q_SIGNALS:
    void roomChanged();
    void showStickersChanged();
    void showEmoticonsChanged();

private:
    NeoChatRoom *m_room;
    bool m_showStickers = true;
    bool m_showEmoticons = true;
};

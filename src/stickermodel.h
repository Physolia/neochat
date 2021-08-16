// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: LGPL-2.0-or-later

#pragma once

#include <QObject>
#include <QAbstractListModel>

class NeoChatRoom;

class StickerModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(NeoChatRoom *room READ room WRITE setRoom NOTIFY roomChanged);
    Q_PROPERTY(QString pack READ pack WRITE setPack NOTIFY packChanged);

public:
    enum Roles {
        Url = Qt::UserRole + 1,
        Body,
    };

    explicit StickerModel(QObject *parent = nullptr);

    void setRoom(NeoChatRoom *room);
    NeoChatRoom *room() const;

    int rowCount(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void postSticker(unsigned int index);

    QString pack() const;
    void setPack(const QString &pack);

Q_SIGNALS:
    void roomChanged();
    void packChanged();

private:
    NeoChatRoom *m_room = nullptr;
    QString m_pack;
};

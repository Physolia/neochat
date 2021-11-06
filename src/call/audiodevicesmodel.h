// SPDX-FileCopyrightText: 2021 Nheko Contributors
// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-FileCopyrightText: 2021 Carl Schwan <carl@carlschwan.eu>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>

typedef struct _GstDevice GstDevice;

class AudioDevicesModel : public QAbstractListModel
{
    Q_OBJECT

public:
    struct AudioSource {
        QString name;
        GstDevice *device;
    };

    AudioDevicesModel(QObject *parent = nullptr);
    ~AudioDevicesModel() = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVector<AudioSource> audioSources() const;

    void addDevice(GstDevice *device);
    bool removeDevice(GstDevice *device, bool changed);
    bool hasMicrophone() const;
    void setDefaultDevice() const;

    GstDevice *currentDevice() const;

private:
    QVector<AudioSource> m_audioSources;
};

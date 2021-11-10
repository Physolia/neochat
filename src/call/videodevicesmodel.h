// SPDX-FileCopyrightText: 2021 Nheko Contributors
// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-FileCopyrightText: 2021 Carl Schwan <carl@carlschwan.eu>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QSize>
#include <optional>

typedef struct _GstDevice GstDevice;

struct VideoSource {
    struct Caps {
        QSize resolution;
        QList<float> framerates;
    };

    QString name;
    GstDevice *device;
    QList<Caps> caps;
};
Q_DECLARE_METATYPE(VideoSource);

class VideoDevicesModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum RoleNames {
        DeviceRole = Qt::UserRole + 1,
    };

    VideoDevicesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addDevice(GstDevice *device);
    bool removeDevice(GstDevice *device, bool changed);

    std::optional<VideoSource> getVideoSource(const QString &cameraName) const;
    QStringList resolutions(const QString &cameraName) const;
    void setDefaultDevice() const;

    bool hasCamera() const;
    GstDevice *currentDevice(QPair<int, int> &resolution, QPair<int, int> &framerate) const;

private:
    QList<VideoSource> m_videoSources;
};

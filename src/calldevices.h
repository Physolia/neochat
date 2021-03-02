/**
 * SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QAbstractListModel>
#include <QList>
#include <QVector>

#include <gst/gst.h>

struct AudioDevice
{
    QString name;
    GstDevice *device;
};

class AudioDevicesModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum RoleNames {
        NameRole = Qt::UserRole + 1,
    };

    AudioDevicesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addDevice(GstDevice *device);
    void remove(const QString &name);

private:
    QList<AudioDevice> m_devices;
};

struct VideoDevice
{
    struct Caps
    {
        QString resolution;
        QList<QString> framerates;
    };

    QString name;
    GstDevice *device;
    QList<Caps> caps;
};
Q_DECLARE_METATYPE(VideoDevice);

class VideoDevicesModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum RoleNames {
        NameRole = Qt::UserRole + 1,
        DeviceRole,
    };

    VideoDevicesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addDevice(GstDevice *device);
    void remove(const QString &name);

private:
    QList<VideoDevice> m_devices;
};

class CallDevices : public QObject
{
    Q_OBJECT

    Q_PROPERTY(AudioDevicesModel *audioDevices READ audioDevicesModel CONSTANT);
    Q_PROPERTY(VideoDevicesModel *videoDevices READ videoDevicesModel CONSTANT);

public:
    static CallDevices &instance()
    {
        static CallDevices _instance;
        return _instance;
    }

    // Don't use these functions! they are public for purely technical reasons
    void busCallback(GstBus *bus, GstMessage *message);
    void addDevice(GstDevice *device);
    void removeDevice(GstDevice *device);

    AudioDevicesModel *audioDevicesModel();
    VideoDevicesModel *videoDevicesModel();

Q_SIGNALS:
    void devicesChanged();

private:
    CallDevices();

    GstDeviceMonitor *m_monitor = nullptr;
    AudioDevicesModel *m_audioDevicesModel;
    VideoDevicesModel *m_videoDevicesModel;
};

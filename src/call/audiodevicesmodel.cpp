// SPDX-FileCopyrightText: 2021 Nheko Contributors
// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-FileCopyrightText: 2021 Carl Schwan <carl@carlschwan.eu>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audiodevicesmodel.h"
#include "neochatconfig.h"
#include <QDebug>

#ifdef GSTREAMER_AVAILABLE
extern "C" {
#include "gst/gst.h"
}
#endif

AudioDevicesModel::AudioDevicesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    qDebug() << "foo";
}

QVariant AudioDevicesModel::data(const QModelIndex &index, int role) const
{
    const auto row = index.row();
    if (role == Qt::DisplayRole) {
        return m_audioSources[row].name;
    }
    return {};
}

int AudioDevicesModel::rowCount(const QModelIndex &parent) const
{
    return m_audioSources.size();
}

GstDevice *AudioDevicesModel::currentDevice() const
{
    const auto config = NeoChatConfig::self();
    const QString name = config->microphone();
    for (const auto &audioSource : m_audioSources) {
        if (audioSource.name == name) {
            qDebug() << "WebRTC: microphone:" << name;
            return audioSource.device;
        }
    }
    qCritical() << "WebRTC: unknown microphone:" << name;
    return nullptr;
}

bool AudioDevicesModel::removeDevice(GstDevice *device, bool changed)
{
    for (int i = 0; i < m_audioSources.size(); i++) {
        if (m_audioSources[i].device == device) {
            Q_EMIT beginRemoveRows({}, i, i);
            m_audioSources.removeAt(i);
            Q_EMIT endRemoveRows();
            return true;
        }
    }
    return false;
}

bool AudioDevicesModel::hasMicrophone() const
{
    return !m_audioSources.empty();
}

void AudioDevicesModel::addDevice(GstDevice *device)
{
    auto _name = gst_device_get_display_name(device);
    QString name(_name);
    g_free(_name);

    qWarning() << "CallDevices: Audio device added:" << name;

    beginInsertRows({}, m_audioSources.size(), m_audioSources.size());
    m_audioSources.append(AudioSource{name, device});
    endInsertRows();
}

void AudioDevicesModel::setCurrentDevice(const QString &device) const
{
    if (NeoChatConfig::microphone().isEmpty()) {
        NeoChatConfig::setMicrophone(m_audioSources.front().name);
    }
}

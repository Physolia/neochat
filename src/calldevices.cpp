/**
 * SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "calldevices.h"

#include <QDebug>
#include <optional>

#include <gst/gst.h>

static gboolean _busCallback(GstBus *bus, GstMessage *message, gpointer userData)
{
    static_cast<CallDevices *>(userData)->busCallback(bus, message);
    return G_SOURCE_CONTINUE;
}

std::optional<QPair<int, int>> getFramerate(const GValue *framerate)
{
    if (GST_VALUE_HOLDS_FRACTION(framerate)) {
        gint numerator = gst_value_get_fraction_numerator(framerate);
        gint denominator = gst_value_get_fraction_denominator(framerate);
        return qMakePair(numerator, denominator);
    }
    return std::nullopt;
}

void addFramerate(QList<QString> &framerates, const QPair<int, int> framerate)
{
    if(static_cast<double>(framerate.first) / framerate.second >= 15.0 /*minimum framerate*/) {
        framerates += QString::number(framerate.first) + QStringLiteral("/") + QString::number(framerate.second);
    }
}

void CallDevices::addDevice(GstDevice *device)
{
    if(!device) {
        return;
    }

    gchar *type = gst_device_get_device_class(device);
    bool isVideo = !strncmp(type, "Video", 5);
    g_free(type);

    if(isVideo) {
        m_videoDevicesModel->addDevice(device);
    } else {
        m_audioDevicesModel->addDevice(device);
    }
}

void CallDevices::removeDevice(GstDevice *device)
{
    gchar *_name = gst_device_get_display_name(device);
    QString name(_name);
    g_free(_name);

    gchar *type = gst_device_get_device_class(device);
    bool isVideo = !strncmp(type, "Video", 5);
    g_free(type);

    qDebug() << "CallDevices:" << (isVideo ? "Video" : "Audio") << "device removed:" << name;

    gst_object_unref(device);

    if(isVideo) {
        m_videoDevicesModel->remove(name);
    } else {
        m_audioDevicesModel->remove(name);
    }
    Q_EMIT devicesChanged();
}

void CallDevices::busCallback(GstBus *bus, GstMessage *message)
{
    switch(GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_DEVICE_ADDED: {
            GstDevice *device;
            gst_message_parse_device_added(message, &device);
            addDevice(device);
            break;
        }
        case GST_MESSAGE_DEVICE_REMOVED: {
            GstDevice *device;
            gst_message_parse_device_removed(message, &device);
            removeDevice(device);
            break;
        }
        case GST_MESSAGE_DEVICE_CHANGED: {
            GstDevice *oldDevice;
            GstDevice *newDevice;
            gst_message_parse_device_changed(message, &newDevice, &oldDevice);
            removeDevice(oldDevice);
            addDevice(newDevice);
            break;
        }
        default:
            break;
    }
}

CallDevices::CallDevices()
{
    m_audioDevicesModel = new AudioDevicesModel(this);
    m_videoDevicesModel = new VideoDevicesModel(this);
    GError *error;
    if(!gst_init_check(nullptr, nullptr, &error)) {
        qWarning() << "Failed to init gstreamer. Calls will not work. Error:" << error->message;
    }
    qDebug() << "GStreamer initialised";

    GstBus *bus;
    GstCaps *caps;
    m_monitor = gst_device_monitor_new();
    qDebug() << m_monitor;
    bus = gst_device_monitor_get_bus(m_monitor);
    gst_bus_add_watch(bus, _busCallback, this);
    gst_object_unref(bus);
    caps = gst_caps_new_empty_simple("video/x-raw");
    gst_device_monitor_add_filter(m_monitor, "Video/Source", caps);
    gst_device_monitor_add_filter(m_monitor, "Video/Duplex", caps);
    gst_caps_unref(caps);

    caps = gst_caps_new_empty_simple("audio/x-raw");
    gst_device_monitor_add_filter(m_monitor, "Audio/Source", caps);
    gst_device_monitor_add_filter(m_monitor, "Audio/Duplex", caps);
    gst_caps_unref(caps);

    if (gst_device_monitor_start(m_monitor)) {
        qDebug() << "Failed to start device monitor";
    }
}

AudioDevicesModel *CallDevices::audioDevicesModel()
{
    return m_audioDevicesModel;
}

AudioDevicesModel::AudioDevicesModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

QVariant AudioDevicesModel::data(const QModelIndex& index, int role) const
{
    auto row = index.row();
    if(role == NameRole) {
        return m_devices[row].name;
    }
    return QStringLiteral("DEADBEEF");
}

int AudioDevicesModel::rowCount(const QModelIndex& parent) const
{
    return m_devices.size();
}

QHash<int, QByteArray> AudioDevicesModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    roles[NameRole] = "name";
    return roles;
}

VideoDevicesModel *CallDevices::videoDevicesModel()
{
    return m_videoDevicesModel;
}

VideoDevicesModel::VideoDevicesModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

QVariant VideoDevicesModel::data(const QModelIndex& index, int role) const
{
    auto row = index.row();
    if(role == NameRole) {
        return m_devices[row].name;
    }
    if(role == DeviceRole) {
        return QVariant::fromValue(m_devices[row]);
    }
    return QStringLiteral("DEADBEEF");
}

int VideoDevicesModel::rowCount(const QModelIndex& parent) const
{
    return m_devices.size();
}

QHash<int, QByteArray> VideoDevicesModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    roles[NameRole] = "name";
    roles[DeviceRole] = "device";
    return roles;
}

void AudioDevicesModel::addDevice(GstDevice *device)
{
    gchar *_name = gst_device_get_display_name(device);
    QString name(_name);
    g_free(_name);

    qDebug() << "CallDevices: Audio device added:" << name;

    Q_EMIT beginInsertRows(QModelIndex(), m_devices.size(), m_devices.size());
    m_devices += AudioDevice{name, device};
    Q_EMIT endInsertRows();

    //TODO remove this hack
    if(name.contains("USB")) {
        m_currentAudioDevice = m_devices.size()-1;
    }
}

void VideoDevicesModel::addDevice(GstDevice *device)
{
    gchar *_name = gst_device_get_display_name(device);
    QString name(_name);
    g_free(_name);

    GstCaps *gstcaps = gst_device_get_caps(device);

    qDebug() << "CallDevices: Video device added:" << name;

    if(!gstcaps) {
        qDebug() << "Unable to get caps for" << name;
        return;
    }

    VideoDevice videoDevice{name, device, {}};
    unsigned int nCaps = gst_caps_get_size(gstcaps);
    for(int i = 0; i < nCaps; i++) {
        GstStructure *structure = gst_caps_get_structure(gstcaps, i);
        const gchar *_capName = gst_structure_get_name(structure);
        if(!strcmp(_capName, "video/x-raw")) {
            gint width, height;
            if(gst_structure_get(structure, "width", G_TYPE_INT, &width, "height", G_TYPE_INT, &height, nullptr)) {
                VideoDevice::Caps caps;
                QPair<int, int> dimension = qMakePair(width, height);
                caps.resolution = QString::number(dimension.first) + QStringLiteral("x") + QString::number(dimension.second);
                QList<QString> framerates;
                const GValue *_framerate = gst_structure_get_value(structure, "framerate");
                if(GST_VALUE_HOLDS_FRACTION(_framerate)) {
                    addFramerate(framerates, *getFramerate(_framerate));
                } else if(GST_VALUE_HOLDS_FRACTION_RANGE(_framerate)) {
                    addFramerate(framerates, *getFramerate(gst_value_get_fraction_range_min(_framerate)));
                    addFramerate(framerates, *getFramerate(gst_value_get_fraction_range_max(_framerate)));
                } else if (GST_VALUE_HOLDS_LIST(_framerate)) {
                    guint nRates = gst_value_list_get_size(_framerate);
                    for(guint j = 0; j < nRates; j++) {
                        const GValue *rate = gst_value_list_get_value(_framerate, j);
                        if(GST_VALUE_HOLDS_FRACTION(rate)) {
                            addFramerate(framerates, *getFramerate(rate));
                        }
                    }
                }
                caps.framerates = framerates;
                videoDevice.caps += caps;
            }
        }
    }
    gst_caps_unref(gstcaps);

    Q_EMIT beginInsertRows(QModelIndex(), m_devices.size(), m_devices.size());
    m_devices += videoDevice;
    Q_EMIT endInsertRows();
}

void AudioDevicesModel::remove(const QString &name)
{
    int i;
    for(i = 0; i < m_devices.size(); i++) {
        if(m_devices[i].name == name) {
            break;
        }
    }
    Q_EMIT beginRemoveRows(QModelIndex(), i, i+1);
    m_devices.removeAt(i);
    Q_EMIT endRemoveRows();
}

void VideoDevicesModel::remove(const QString &name)
{
    int i;
    for(i = 0; i < m_devices.size(); i++) {
        if(m_devices[i].name == name) {
            break;
        }
    }
    Q_EMIT beginRemoveRows(QModelIndex(), i, i+1);
    m_devices.removeAt(i);
    Q_EMIT endRemoveRows();
}

GstDevice * AudioDevicesModel::currentDevice() const
{
    return m_devices[m_currentAudioDevice].device; //TODO make device selectable
}

GstDevice * VideoDevicesModel::videoDevice(QPair<int, int>& resolution, QPair<int, int>& framerate)
{//TODO save as pair
    auto res = m_devices[0].caps[0].resolution;
    auto index = res.indexOf("x");
    resolution = qMakePair(res.left(index).toInt(), res.mid(index+1).toInt());
    auto fr = m_devices[0].caps[0].resolution;
    index = fr.indexOf("/");
    framerate = qMakePair(fr.left(index).toInt(), fr.mid(index+1).toInt());
    return m_devices[0].device;
}

// SPDX-FileCopyrightText: 2021 Nheko Contributors
// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-FileCopyrightText: 2021 Carl Schwan <carl@carlschwan.eu>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "videodevicesmodel.h"

#include "neochatconfig.h"

#ifdef GSTREAMER_AVAILABLE
extern "C" {
#include "gst/gst.h"
}
#endif

namespace
{

using Framerate = std::pair<int, int>;

std::optional<Framerate> getFramerate(const GValue *value)
{
    if (GST_VALUE_HOLDS_FRACTION(value)) {
        gint num = gst_value_get_fraction_numerator(value);
        gint den = gst_value_get_fraction_denominator(value);
        return Framerate{num, den};
    }
    return std::nullopt;
}

void addFramerate(QStringList &rates, const Framerate &rate)
{
    constexpr double minimumFramerate = 1.0;
    if (static_cast<double>(rate.first) / rate.second >= minimumFramerate)
        rates.push_back(rate.first + "/" + rate.second);
}

QPair<int, int> tokenise(QStringView str, QChar delim)
{
    QPair<int, int> ret;
    auto pos = str.indexOf(delim);
    ret.first = str.left(pos).toInt();
    ret.second = str.right(str.length() - pos + 1).toInt();
    return ret;
}
}

VideoDevicesModel::VideoDevicesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

QVariant VideoDevicesModel::data(const QModelIndex &index, int role) const
{
    if (index.isValid()) {
        return {};
    }
    const auto row = index.row();
    switch (role) {
    case Qt::DisplayRole:
        return m_videoSources[row].name;
    case DeviceRole:
        return QVariant::fromValue(m_videoSources[row]);
    }
    return {};
}

int VideoDevicesModel::rowCount(const QModelIndex &) const
{
    return m_videoSources.size();
}

QHash<int, QByteArray> VideoDevicesModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    roles[DeviceRole] = QByteArrayLiteral("device");
    return roles;
}

void VideoDevicesModel::addDevice(GstDevice *device)
{
    gchar *_name = gst_device_get_display_name(device);
    QString name(_name);
    g_free(_name);

    GstCaps *gstcaps = gst_device_get_caps(device);

    qDebug() << "CallDevices: Video device added:" << name;

    if (!gstcaps) {
        qDebug() << "Unable to get caps for" << name;
        return;
    }

    VideoSource videoSource{name, device, {}};
    for (size_t i = 0; i < gst_caps_get_size(gstcaps); i++) {
        GstStructure *structure = gst_caps_get_structure(gstcaps, i);
        const gchar *_capName = gst_structure_get_name(structure);
        if (!strcmp(_capName, "video/x-raw")) {
            gint width, height;
            if (gst_structure_get(structure, "width", G_TYPE_INT, &width, "height", G_TYPE_INT, &height, nullptr)) {
                VideoSource::Caps caps;
                caps.resolution = QString::number(width) + QStringLiteral("x") + QString::number(height);
                QStringList framerates;
                const GValue *_framerate = gst_structure_get_value(structure, "framerate");
                if (GST_VALUE_HOLDS_FRACTION(_framerate)) {
                    addFramerate(framerates, *getFramerate(_framerate));
                } else if (GST_VALUE_HOLDS_FRACTION_RANGE(_framerate)) {
                    addFramerate(framerates, *getFramerate(gst_value_get_fraction_range_min(_framerate)));
                    addFramerate(framerates, *getFramerate(gst_value_get_fraction_range_max(_framerate)));
                } else if (GST_VALUE_HOLDS_LIST(_framerate)) {
                    guint nRates = gst_value_list_get_size(_framerate);
                    for (guint j = 0; j < nRates; j++) {
                        const GValue *rate = gst_value_list_get_value(_framerate, j);
                        if (GST_VALUE_HOLDS_FRACTION(rate)) {
                            addFramerate(framerates, *getFramerate(rate));
                        }
                    }
                }
                caps.framerates = framerates;
                videoSource.caps += caps;
            }
        }
    }
    gst_caps_unref(gstcaps);

    Q_EMIT beginInsertRows({}, m_videoSources.size(), m_videoSources.size());
    m_videoSources.append(videoSource);
    Q_EMIT endInsertRows();
}

bool VideoDevicesModel::removeDevice(GstDevice *device, bool changed)
{
    for (int i = 0; i < m_videoSources.size(); i++) {
        if (m_videoSources[i].device == device) {
            beginRemoveRows(QModelIndex(), i, i);
            m_videoSources.removeAt(i);
            endRemoveRows();
            return true;
        }
    }
    return false;
}

GstDevice *VideoDevicesModel::currentDevice(QPair<int, int> &resolution, QPair<int, int> &framerate) const
{
    const auto config = NeoChatConfig::self();
    if (auto s = getVideoSource(config->camera()); s) {
        qDebug() << "WebRTC: camera:" << config->camera();
        resolution = tokenise(config->cameraResolution(), 'x');
        framerate = tokenise(config->cameraFramerate(), '/');
        qDebug() << "WebRTC: camera resolution:" << resolution.first << 'x' << resolution.second;
        qDebug() << "WebRTC: camera frame rate:" << framerate.first << '/' << framerate.second;
        return s->device;
    } else {
        qCritical() << "WebRTC: unknown camera:" << config->camera();
        return nullptr;
    }
}

void VideoDevicesModel::setDefaultDevice() const
{
    if (NeoChatConfig::camera().isEmpty()) {
        const VideoSource &camera = m_videoSources.front();
        NeoChatConfig::setCamera(camera.name);
        NeoChatConfig::setCameraResolution(camera.caps.front().resolution);
        NeoChatConfig::setCameraFramerate(camera.caps.front().frameraetes.front());
    }
}

std::optional<VideoSource> VideoDevicesModel::getVideoSource(const QString &cameraName) const
{
    for (const auto &videoSource : m_videoSources) {
        if (videoSource.name == cameraName) {
            return videoSource;
        }
    }
    return std::nullopt;
}

QStringList VideoDevicesModel::resolutions(const QString &cameraName) const
{
    QStringList ret;
    if (auto s = getVideoSource(cameraName); s) {
        ret.reserve(s->caps.size());
        for (const auto &c : s->caps) {
            ret.push_back(c.resolution);
        }
    }
    return ret;
}

bool VideoDevicesModel::hasCamera() const
{
    return !m_videoSources.isEmpty();
}

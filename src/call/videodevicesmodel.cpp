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

using FrameRate = std::pair<int, int>;

std::optional<FrameRate> getFrameRate(const GValue *value)
{
    if (GST_VALUE_HOLDS_FRACTION(value)) {
        gint num = gst_value_get_fraction_numerator(value);
        gint den = gst_value_get_fraction_denominator(value);
        return FrameRate{num, den};
    }
    return std::nullopt;
}

void addFrameRate(QStringList &rates, const FrameRate &rate)
{
    constexpr double minimumFrameRate = 15.0;
    if (static_cast<double>(rate.first) / rate.second >= minimumFrameRate)
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

int VideoDevicesModel::rowCount(const QModelIndex &parent) const
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
    unsigned int nCaps = gst_caps_get_size(gstcaps);
    for (int i = 0; i < nCaps; i++) {
        GstStructure *structure = gst_caps_get_structure(gstcaps, i);
        const gchar *_capName = gst_structure_get_name(structure);
        if (!strcmp(_capName, "video/x-raw")) {
            gint width, height;
            if (gst_structure_get(structure, "width", G_TYPE_INT, &width, "height", G_TYPE_INT, &height, nullptr)) {
                VideoSource::Caps caps;
                QPair<int, int> dimension = qMakePair(width, height);
                caps.resolution = QString::number(dimension.first) + QStringLiteral("x") + QString::number(dimension.second);
                QStringList framerates;
                const GValue *_framerate = gst_structure_get_value(structure, "framerate");
                if (GST_VALUE_HOLDS_FRACTION(_framerate)) {
                    addFrameRate(framerates, *getFrameRate(_framerate));
                } else if (GST_VALUE_HOLDS_FRACTION_RANGE(_framerate)) {
                    addFrameRate(framerates, *getFrameRate(gst_value_get_fraction_range_min(_framerate)));
                    addFrameRate(framerates, *getFrameRate(gst_value_get_fraction_range_max(_framerate)));
                } else if (GST_VALUE_HOLDS_LIST(_framerate)) {
                    guint nRates = gst_value_list_get_size(_framerate);
                    for (guint j = 0; j < nRates; j++) {
                        const GValue *rate = gst_value_list_get_value(_framerate, j);
                        if (GST_VALUE_HOLDS_FRACTION(rate)) {
                            addFrameRate(framerates, *getFrameRate(rate));
                        }
                    }
                }
                caps.frameRates = framerates;
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

GstDevice *VideoDevicesModel::currentDevice(QPair<int, int> &resolution, QPair<int, int> &frameRate) const
{
    const auto config = NeoChatConfig::self();
    if (auto s = getVideoSource(config->camera()); s) {
        qDebug() << "WebRTC: camera:" << config->camera();
        resolution = tokenise(config->cameraResolution(), 'x');
        frameRate = tokenise(config->cameraFrameRate(), '/');
        qDebug() << "WebRTC: camera resolution:" << resolution.first << 'x' << resolution.second;
        qDebug() << "WebRTC: camera frame rate:" << frameRate.first << '/' << frameRate.second;
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
        NeoChatConfig::setCameraFrameRate(camera.caps.front().frameRates.front());
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

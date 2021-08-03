// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtLocation 5.15
import QtPositioning 5.15

import org.kde.neochat 1.0
import NeoChat.Component 1.0
import NeoChat.Dialog 1.0
import NeoChat.Menu.Timeline 1.0

Map {
    Layout.fillWidth: true
    Layout.preferredHeight: 200

    property string latlong: model.content.geo_uri.split(':')[1]
    property string latitude: latlong.split(',')[0]
    property string longitude: latlong.split(',')[1]
    center: QtPositioning.coordinate(latitude, longitude)
    zoomLevel: 15
    plugin: Plugin {
        name: "osm"
    }
    MapCircle {
        radius: 100
        color: "#46a2da"
        border.color: "#190a33"
        border.width: 2
        smooth: true
        opacity: 0.25
        center: QtPositioning.coordinate(latitude, longitude)
    }
    onCopyrightLinkActivated: {
        Qt.openUrlExternally(link)
    }
}
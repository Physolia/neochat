// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtLocation 5.15
import QtPositioning 5.15

import org.kde.kirigami 2.15 as Kirigami

import org.kde.neochat 1.0

ColumnLayout {
    id: column

    property string latlong: model.content.geo_uri.split(':')[1]
    property string latitude: latlong.split(',')[0]
    property string longitude: latlong.split(',')[1]

    property string formattedBody: model.content.formatted_body

    Layout.fillWidth: true
    Layout.preferredHeight: 250

    Map {
        id: map
        Layout.fillHeight: true
        Layout.fillWidth: true

        center: QtPositioning.coordinate(column.latitude, column.longitude)
        zoomLevel: 15
        plugin: Plugin {
            name: "osm"
        }
        MapCircle {
            radius: 1500 / map.zoomLevel
            color: Kirigami.Theme.highlightColor
            border.color: Kirigami.Theme.linkColor
            border.width: Kirigami.Units.devicePixelRatio * 2
            smooth: true
            opacity: 0.25
            center: QtPositioning.coordinate(latitude, longitude)
        }
        onCopyrightLinkActivated: {
            Qt.openUrlExternally(link)
        }
        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: {
                fullScreenMap.createObject(parent, {"content": model.content})
            }
            onLongPressed: openMessageContext(author, model.message, eventId, toolTip, eventType, model.formattedBody ?? model.body, parent.selectedText)
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: openMessageContext(author, model.message, eventId, toolTip, eventType, model.formattedBody ?? model.body, parent.selectedText)
        }
    }
    TextEdit {
        Layout.fillWidth: true

        readOnly: true
        text: column.formattedBody
        wrapMode: Text.WordWrap
        textFormat: Text.RichText
        selectByMouse: !Kirigami.Settings.isMobile

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: openMessageContext(author, model.message, eventId, toolTip, eventType, model.formattedBody ?? model.body, parent.selectedText)
        }
        TapHandler {
            acceptedButtons: Qt.LeftButton
            onLongPressed: openMessageContext(author, model.message, eventId, toolTip, eventType, model.formattedBody ?? model.body, parent.selectedText)
        }
        onLinkActivated: {
            Qt.openUrlExternally(link)
        }
        onHoveredLinkChanged: {
            if (hoveredLink.length > 0) {
                applicationWindow().hoverLinkIndicator.text = hoveredLink;
            } else {
                applicationWindow().hoverLinkIndicator.text = "";
            }
        }
        HoverHandler {
            cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.IBeamCursor
        }
    }
}
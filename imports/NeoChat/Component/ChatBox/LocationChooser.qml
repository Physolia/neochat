// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtLocation 5.15
import QtPositioning 5.15

import org.kde.kirigami 2.15 as Kirigami

Kirigami.OverlaySheet {
    id: sheet

    header: RowLayout {
        Kirigami.SearchField {
            placeholderText: i18n("Search location…")
            Layout.fillWidth: true
            onAccepted: {
                search.query = text
                search.update()
                search.goToFirstResult()
            }
        }
        ToolButton {
            id: button
            text: i18n("Search")
            icon.name: "search"
            onClicked: {
                search.query = text
                search.update()
                search.goToFirstResult()
            }
        }
    }

    footer: RowLayout {
        layoutDirection: Qt.RightToLeft
        ToolButton {
            text: i18n("Send")
            icon.name: "document-send"
            onClicked: console.warn("foo")
        }
        ToolButton {
            text: i18n("Cancel")
            icon.name: "dialog-cancel"
            onClicked: sheet.close()
        }
    }

    ColumnLayout {
        width: 800
        height: 600
        Map {
            id: map
            Layout.fillWidth: true
            Layout.fillHeight: true
            plugin: Plugin {
                name: "osm"
            }

            MapItemView {
                id: locationMap

                property string text: ""

                model: GeocodeModel {
                    id: geocodeModel
                    plugin: map.plugin
                    onLocationsChanged: locationMap.text = geocodeModel.get(0).address.text
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        var coords = map.toCoordinate(Qt.point(mouseX, mouseY), false)
                        geocodeModel.query = coords
                        geocodeModel.update()
                    }
                }

                delegate: MapCircle {
                    id: point
                    radius: 1500 / map.zoomLevel
                    color: Kirigami.Theme.highlightColor
                    border.color: Kirigami.Theme.linkColor
                    border.width: Kirigami.Units.devicePixelRatio * 2
                    smooth: true
                    opacity: 0.25
                    center: locationData.coordinate
                    ToolTip.visible: mouse.containsMouse
                    ToolTip.delay: 300
                    ToolTip.text: model.locationData.address.text
                    MouseArea {
                        id: mouse
                        hoverEnabled: true
                        anchors.fill: parent
                    }
                }
            }
            MapItemView {
                id: search

                property alias query: searchModel.query

                function update() {
                    searchModel.update()
                }

                function clear() {
                    searchModel.reset()
                }

                function goToFirstResult() {
                    if(searchModel.count > 0) {
                        map.center.latitude = searchModel.get(0).coordinate.latitude
                        map.center.longitude = searchModel.get(0).coordinate.longitude
                    }
                    map.zoomLevel = 15
                }

                model: GeocodeModel {
                    id: searchModel
                    plugin: map.plugin
                }

                delegate: pointDelegate
                Component {
                    id: pointDelegate
                    MapCircle {
                        id: point
                        radius: 1500 / map.zoomLevel
                        color: Kirigami.Theme.highlightColor
                        border.color: Kirigami.Theme.linkColor
                        border.width: Kirigami.Units.devicePixelRatio * 2
                        smooth: true
                        opacity: 0.25
                        center: locationData.coordinate
                        ToolTip.visible: mouse.containsMouse
                        ToolTip.delay: 300
                        ToolTip.text: model.locationData.address.text
                        MouseArea {
                            id: mouse
                            hoverEnabled: true
                            anchors.fill: parent
                        }
                    }
                }
            }
        }
        Kirigami.FormLayout {
            Label {
                Kirigami.FormData.label: i18n("Position:")
                text: i18n("Latitude: %1, Longitude: %2", geocodeModel.query.latitude, geocodeModel.query.longitude)
            }
            TextField {
                id: description
                Kirigami.FormData.label: i18n("Description:")
                Connections {
                    target: geocodeModel
                    function onLocationsChanged() {
                        description.placeholderText = geocodeModel.get(0).address.text
                    }
                }
            }
        }
    }
}
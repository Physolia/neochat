// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.15 as Kirigami

import org.kde.neochat 1.0

import NeoChat.Component 1.0

ColumnLayout {
    id: stickerPicker

    spacing: 0

    ListView {
        id: pack

        property string selectedPack

        Layout.fillWidth: true
        Layout.preferredHeight: Kirigami.Units.gridUnit * 2

        boundsBehavior: Flickable.DragOverBounds

        clip: true

        orientation: ListView.Horizontal

        model: ImagePacksModel {
            room: currentRoom
            showStickers: true
            showEmoticons: false
        }

        delegate: ItemDelegate {
            id: del

            required property string displayName
            required property string avatarUrl
            required property string id

            height: Kirigami.Units.gridUnit * 2
            width: height

            contentItem: Image {
                source: "image://mxc/" + del.avatarUrl

            }

            Rectangle {
                anchors.bottom: parent.bottom

                width: parent.width
                height: 2

                visible: pack.selectedPack === del.id

                color: Kirigami.Theme.focusColor
            }

            onClicked: pack.selectedPack = id
        }
    }

    GridView {
        id: grid
        Layout.fillWidth: true
        Layout.preferredHeight: Kirigami.Units.gridUnit * 8
        Layout.fillHeight: true

        cellWidth: width / 5
        cellHeight: cellWidth

        boundsBehavior: Flickable.DragOverBounds

        clip: true

        model: StickerModel {
            id: stickerModel
            pack: pack.selectedPack
            room: currentRoom
        }

        delegate: ItemDelegate {
            width: grid.cellWidth
            height: width

            Image {
                source: "image://mxc:/" + model.url
                anchors.fill: parent
                anchors.margins: 2

                sourceSize.width: width
                sourceSize.height: height

                Rectangle {
                    anchors.fill: parent
                    visible: parent.status === Image.Loading
                    radius: height/2
                    gradient: ShimmerGradient { }
                }
            }

            onClicked: stickerModel.postSticker(model.index)
        }

        ScrollBar.vertical: ScrollBar {}
    }
}

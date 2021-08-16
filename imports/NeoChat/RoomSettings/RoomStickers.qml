// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.15 as Kirigami

import org.kde.neochat 1.0
import NeoChat.Component 1.0
import NeoChat.Dialog 1.0

Kirigami.ScrollablePage {
    id: root

    property var room

    title: i18n('Stickers & Emoticons')

    ListView {
        model: ImagePacksModel {
            room: root.room
            showStickers: true
            showEmoticons: true
        }
        delegate: Kirigami.BasicListItem {
            text: model.displayName
            reserveSpaceForSubtitle: true
            leading: Image {
                width: height
                sourceSize.width: width
                sourceSize.height: height
                source: "image://mxc/" + model.avatarUrl

                Rectangle {
                    anchors.fill: parent
                    visible: parent.status === Image.Loading
                    radius: height / 2
                    gradient: ShimmerGradient { }
                }
            }

            trailing: ToolButton {
                width: height
                icon.name: "delete"
                onClicked: console.warn("TODO")
            }

            onClicked: pageStack.pushDialogLayer("qrc:/imports/NeoChat/RoomSettings/ImagePackEditor.qml", {
                room: root.room,
                packId: model.id
            })
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            text: i18n("No image packs found")
            visible: parent.count === 0
        }
    }

    footer: ToolBar {
        contentItem: RowLayout {
            Item {
                Layout.fillWidth: true
            }
            Button {
                Layout.alignment: Qt.AlignRight
                enabled: false
                text: i18n("Apply")
            }
        }
    }
}


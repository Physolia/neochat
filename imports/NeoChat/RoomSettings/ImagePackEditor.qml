// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.15 as Kirigami

import org.kde.neochat 1.0
import NeoChat.Component 1.0 as Components
import NeoChat.Dialog 1.0

Kirigami.ScrollablePage {
    id: root

    property var room
    property string packId

    title: i18nc('@title:window', 'Edit image pack')

    ColumnLayout {
        anchors.fill: parent

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                clip: true
                model: StickerModel {
                    id: stickerModel
                    room: root.room
                    pack: root.packId
                }

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    text: i18n("This sticker pack is empty")
                    visible: parent.count === 0
                }

                delegate: Kirigami.BasicListItem {
                    text: model.body
                    reserveSpaceForSubtitle: true

                    leading: Image {
                        width: height
                        sourceSize.width: width
                        sourceSize.height: height
                        source: "image://mxc:/" + model.url

                        Rectangle {
                            anchors.fill: parent
                            visible: parent.status === Image.Loading
                            radius: height/2
                            gradient: Components.ShimmerGradient { }
                        }
                    }

                    trailing: ToolButton {
                        width: height
                        icon.name: "delete"
                        onClicked: stickerModel.removeEmoji(del.name)
                    }
                }
            }
        }

        Loader {
            active: pageSettingStack.wideMode
            sourceComponent: addEmojiComponent
            Layout.fillWidth: true
        }
    }


    footer: ToolBar {
        position: ToolBar.Footer
        RowLayout {
            anchors.fill: parent
            Item {
                Layout.fillWidth: Qt.application.layoutDirection == Qt.LeftToRight
            }

            TextField {
                id: stickerField
                placeholderText: i18n("Sticker name")
            }

            Button {
                text: i18n("Add Sticker…")

                enabled: stickerField.text != ""
                property var fileDialog: null

                onClicked: {
                    if (this.fileDialog != null) {
                        return;
                    }

                    this.fileDialog = openFileDialog.createObject(Overlay.overlay)

                    this.fileDialog.chosen.connect((url) => {
                        stickerModel.addEmoji(emojiField.text, url)
                        this.fileDialog = null
                    })
                    this.fileDialog.onRejected.connect(() => {
                        this.fileDialog = null
                    })
                    this.fileDialog.open()
                }
            }
            Item {
                Layout.fillWidth: Qt.application.layoutDirection == Qt.RightToLeft
            }
        }
    }
    Component {
        id: openFileDialog

        OpenFileDialog {
            folder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
        }
    }
}


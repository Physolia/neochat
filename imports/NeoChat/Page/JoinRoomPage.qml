// SPDX-FileCopyrightText: 2019 Black Hat <bhat@encom.eu.org>
// SPDX-FileCopyrightText: 2020 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: GPL-3.0-only

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.15 as Kirigami

import org.kde.neochat 1.0
import NeoChat.Component 1.0

Kirigami.ScrollablePage {
    id: root
    property var connection

    property alias keyword: identifierField.text
    property string server

    title: i18n("Explore Rooms")

    Component.onCompleted: identifierField.forceActiveFocus()

    header: Control {
        padding: Kirigami.Units.largeSpacing
        contentItem: RowLayout {
            Kirigami.SearchField {
                id: identifierField
                property bool isRoomAlias: text.match(/#(.+):(.+)/g)
                property var room: isRoomAlias ? connection.roomByAlias(text) : null
                property bool isJoined: room != null

                Layout.fillWidth: true

                placeholderText: i18n("Find a room...")
            }

            Button {
                id: joinButton

                visible: identifierField.isRoomAlias

                text: identifierField.isJoined ? i18n("View") : i18n("Join")
                highlighted: true

                onClicked: {
                    if (!identifierField.isJoined) {
                        Controller.joinRoom(identifierField.text);
                        // When joining the room, the room will be opened
                    }
                    applicationWindow().pageStack.layers.pop();
                }
            }

            ComboBox {
                Layout.maximumWidth: 120

                id: serverField

                editable: currentIndex == 1

                model: [i18n("Local"), i18n("Global"), "matrix.org"]

                onCurrentIndexChanged: {
                    if (currentIndex == 0) {
                        server = ""
                    } else if (currentIndex == 2) {
                        server = "matrix.org"
                    }
                }

                Keys.onReturnPressed: {
                    if (currentIndex == 1) {
                        server = editText
                    }
                }
            }
        }
    }

    ListView {
        id: publicRoomsListView
        clip: true
        model: PublicRoomListModel {
            id: publicRoomListModel

            connection: root.connection
            server: root.server
            keyword: root.keyword
        }

        onContentYChanged: {
            if(publicRoomListModel.hasMore && contentHeight - contentY < publicRoomsListView.height + 200)
                publicRoomListModel.next();
        }
        delegate: RoomListItem {
            connection: root.connection
        }
    }
}

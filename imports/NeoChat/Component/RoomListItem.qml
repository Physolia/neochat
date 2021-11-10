// SPDX-FileCopyrightText: 2019 Black Hat <bhat@encom.eu.org>
// SPDX-FileCopyrightText: 2020 Carl Schwan <carl@carlschwan.eu>
// SPDX-FileCopyrightText: 2021 Smitty van Bodegom <me@smitop.com>
// SPDX-License-Identifier: GPL-3.0-only

// Used to display Space children and public room lists.

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.15 as Kirigami

import org.kde.neochat 1.0
import NeoChat.Component 1.0

Kirigami.AbstractListItem {
    property bool justJoined: false
    required property var connection

    width: parent.width
    onClicked: {
        if (!isJoined) {
            Controller.joinRoom(roomID)
            justJoined = true;
        } else {
            RoomManager.enterRoom(connection.room(roomID))
        }
        applicationWindow().pageStack.layers.pop();
    }
    contentItem: RowLayout {
        Kirigami.Avatar {
            Layout.preferredWidth: Kirigami.Units.gridUnit * 2
            Layout.preferredHeight: Kirigami.Units.gridUnit * 2

            source: model.avatar ? ("image://mxc/" + model.avatar) : ""
            name: name
        }
        ColumnLayout {
            Layout.fillWidth: true
            RowLayout {
                Layout.fillWidth: true
                Kirigami.Heading {
                    Layout.fillWidth: true
                    level: 4
                    text: name
                    font.bold: true
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                }
                Label {
                    visible: isJoined || justJoined
                    text: i18n("Joined")
                    color: Kirigami.Theme.linkColor
                }
            }
            Label {
                Layout.fillWidth: true
                visible: text
                text: topic ? topic.replace(/(\r\n\t|\n|\r\t)/gm," ") : ""
                textFormat: Text.PlainText
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }
            RowLayout {
                Layout.fillWidth: true
                Kirigami.Icon {
                    source: "user"
                    color: Kirigami.Theme.disabledTextColor
                    implicitHeight: Kirigami.Units.iconSizes.small
                    implicitWidth: Kirigami.Units.iconSizes.small
                }
                Label {
                    text: memberCount + " " + (alias ?? roomID)
                    color: Kirigami.Theme.disabledTextColor
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }
    }
}

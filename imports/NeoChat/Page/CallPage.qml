/* SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import org.kde.kirigami 2.14 as Kirigami

import org.kde.neochat 1.0

Kirigami.Page {

    contextualActions: [
        Kirigami.Action {
            text: i18n("Configure input devices")
            iconName: "settings-configure"
            onTriggered: ; //TODO
        }
    ]

    title: CallManager.hasInvite ? i18n("Incoming Call")
            : CallManager.isInviting ? i18n("Calling")
            : CallManager.state == CallSession.Initiating ? i18n("Configuring Call")
            : i18n("Call")

    ColumnLayout {
        anchors.centerIn: parent
        Kirigami.Avatar {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Kirigami.Units.gridUnit * 20
            Layout.preferredHeight: Kirigami.Units.gridUnit * 20
            source: CallManager.remoteUser.avatarMediaId ? ("image://mxc/" + CallManager.remoteUser.avatarMediaId) : undefined
        }
        Label {
            text: CallManager.remoteUser.displayName
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Text.AlignHCenter
        }
        Label {
            text: CallManager.room.displayName
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Text.AlignHCenter
        }
        ColumnLayout {
            visible: CallManager.hasInvite
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            Button {
                text: i18n("Accept")
                Layout.alignment: Qt.AlignHCenter
                onClicked: {
                    CallManager.acceptCall()
                }
            }
            Button {
                text: i18n("Decline")
                Layout.alignment: Qt.AlignHCenter
                onClicked: {
                    CallManager.hangupCall()
                }
            }
            Button {
                text: i18n("Ignore")
                Layout.alignment: Qt.AlignHCenter
                onClicked: {
                    CallManager.ignoreCall()
                    pageStack.push()
                }
            }
        }
        ColumnLayout {
            visible: CallManager.isInviting || CallManager.state == CallSession.CONNECTED
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Button {
                text: i18n("Hang up")
                Layout.alignment: Qt.AlignHCenter
                onClicked: {
                    CallManager.hangupCall()
                }
            }
        }
    }

    Timer {
        id: lifeTimer
        interval: CallManager.lifetime
        onTriggered: {
            pageStack.pop()
        }
    }

    Connections {
        target: CallManager
        function onCallEnded() {
            pageStack.pop()
        }
        function onHasInviteChanged() {
            if(!CallManager.hasInvite) {
                lifeTimer.stop()
            }
        }
        function onIsInvitingChanged() {
            if(!CallManager.isInviting) {
                lifeTimer.stop()
            }
        }
    }

    Component.onCompleted: {
        if(CallManager.hasInvite || CallManager.isInviting) {
            lifeTimer.start()
        }
    }
}

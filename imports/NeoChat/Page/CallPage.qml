/* SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick 2.12
import QtQuick.Controls 2.12 as QQC2
import QtQuick.Layouts 1.12

import org.kde.kirigami 2.14 as Kirigami
import org.freedesktop.gstreamer.GLVideoItem 1.0

import org.kde.neochat 1.0
import NeoChat.Component.Call 1.0

Kirigami.Page {
    id: page

    title: CallManager.hasInvite ? i18n("Incoming Call")
            : CallManager.isInviting ? i18n("Calling")
            : CallManager.state == CallSession.Initiating ? i18n("Configuring Call")
            : i18n("Call")

    ColumnLayout {
        anchors.fill: parent

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

            GstGLVideoItem {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Component.onCompleted: CallManager.item = this
                z: 10
                MouseArea {
                    anchors.fill: parent
                    drag.target: parent
                }
            }

            Kirigami.Avatar {
                Layout.preferredWidth: Kirigami.Units.iconSizes.huge
                Layout.preferredHeight: Kirigami.Units.iconSizes.huge
                Layout.alignment: Qt.AlignHCenter

                name: CallManager.room.displayName
                source: "image://mxc/" + CallManager.room.avatarMediaId
            }

            QQC2.Label {
                text: CallManager.remoteUser.displayName

                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
            QQC2.Label {
                text: CallManager.room.displayName

                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            Item { implicitHeight: Kirigami.Units.gridUnit * 2 }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter

                id: buttonRow
                spacing: Kirigami.Units.gridUnit

                CallPageButton {
                    text: i18n("Accept")
                    icon.name: "call-start"
                    shimmering: true
                    temprament: CallPageButton.Constructive
                    visible: CallManager.globalState === CallManager.INCOMING

                    onClicked: CallManager.acceptCall()
                }
                CallPageButton {
                    text: checked ? i18n("Enable Camera") : i18n("Disable Camera")
                    icon.name: checked ? "camera-off" : "camera-on"
                    checkable: true
                }
                CallPageButton {
                    text: checked ? i18n("Unmute Speaker") : i18n("Mute Speaker")
                    icon.name: checked ? "audio-volume-muted" : "audio-speakers-symbolic"
                    checkable: true
                }
                CallPageButton {
                    text: checked ? i18n("Unmute Microphone") : i18n("Mute Microphone")
                    icon.name: checked ? "microphone-sensitivity-muted" : "microphone-sensitivity-high"
                    checkable: true
                    checked: CallManager.muted

                    onToggled: CallManager.muted = !CallManager.muted
                }
                CallPageButton {
                    text: i18n("Configure Devices")
                    icon.name: "settings-configure"
                    onClicked: callConfigurationSheet.open()
                }
                CallPageButton {
                    id: denyButton
                    visible: CallManager.globalState === CallManager.INCOMING
                    text: i18n("Deny")
                    icon.name: "call-stop"
                    shimmering: true
                    temprament: CallPageButton.Destructive

                    onClicked: CallManager.hangupCall()
                }
                CallPageButton {
                    visible: !denyButton.visible
                    text: CallManager.isInviting ? i18n("Cancel") : i18n("Hang Up")
                    icon.name: "call-stop"
                    shimmering: CallManager.isInviting
                    temprament: CallPageButton.Destructive

                    onClicked: CallManager.hangupCall()
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

    CallConfigurationSheet {
        id: callConfigurationSheet
    }
}

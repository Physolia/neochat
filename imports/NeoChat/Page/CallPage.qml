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
        anchors.fill: parent

        GstGLVideoItem {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Component.onCompleted: Controller.item = this
            z: 10
            MouseArea {
                anchors.fill: parent
                drag.target: parent
            }
        }

        QQC2.Label {
            text: CallManager.remoteUser.displayName
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Text.AlignHCenter
        }
        QQC2.Label {
            text: CallManager.room.displayName
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Text.AlignHCenter
        }

        // controls
        RowLayout {
            //opacity: callStatus === DialerUtils.Active ? 1 : 0
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: Kirigami.Units.gridUnit * 32
            Layout.minimumHeight: Kirigami.Units.gridUnit * 3.5
            id: buttonRow
            spacing: Kirigami.Units.smallSpacing
            CallPageButton {
                id: dialerButton
                Layout.fillHeight: true
                Layout.fillWidth: true
                iconSource: "dialog-messages"
                text: i18n("Go back to the chat")
                toggledOn: false
                // TODO onClicked: toggledOn = !toggledOn
            }
            CallPageButton {
                id: speakerButton
                Layout.fillHeight: true
                Layout.fillWidth: true
                iconSource: "audio-speakers-symbolic"
                text: i18n("Speaker")
                toggledOn: false
                onClicked: toggledOn = !toggledOn
                onToggledOnChanged: {
                    //DialerUtils.setSpeakerMode(toggledOn);
                }
            }
            CallPageButton {
                id: muteButton

                Layout.fillHeight: true
                Layout.fillWidth: true

                iconSource: toggledOn ? "microphone-sensitivity-muted-symbolic" : "microphone-sensitivity-high-symbolic"
                text: i18n("Mute")
                toggledOn: CallManager.muted
                onClicked: CallManager.muted = !CallManager.muted

                onToggledOnChanged: {
                    //DialerUtils.setMute(toggledOn)
                }
            }
        }

        //RowLayout {
            //visible: CallManager.isInviting || CallManager.state == CallSession.CONNECTED
            //Layout.alignment: Qt.AlignHCenter
            //Layout.fillWidth: true
            //CallPageButton {
                //text: i18n("Hang up")
                //Layout.alignment: Qt.AlignHCenter
                //onClicked: {
                    //CallManager.hangupCall()
                //}
            //}
        //}

        Item {
            Layout.minimumHeight: Kirigami.Units.gridUnit * 5
            Layout.fillWidth: true

            AnswerSwipe {
                anchors.fill: parent
                //STATUS_INCOMING
                visible: CallManager.hasInvite
                onAccepted: CallManager.acceptCall()
                onRejected: CallManager.hangupCall()
                // TODO CallManager.ignoreCall()
            }

            // end call button
            QQC2.AbstractButton {
                id: endCallButton
                //STATUS_ACTIVE
                visible: CallManager.isInviting || CallManager.state == CallSession.CONNECTED

                anchors.centerIn: parent
                width: Kirigami.Units.gridUnit * 3.5
                height: Kirigami.Units.gridUnit * 3.5

                onClicked: CallManager.hangupCall()

                background: Rectangle {
                    anchors.centerIn: parent
                    height: Kirigami.Units.gridUnit * 3.5
                    width: height
                    radius: height / 2

                    color: "red"
                    opacity: endCallButton.pressed ? 0.5 : 1

                    Kirigami.Icon {
                        source: "call-stop"
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.largeSpacing
                        color: "white"
                        isMask: true
                    }
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

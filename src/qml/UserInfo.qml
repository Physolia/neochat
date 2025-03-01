// SPDX-FileCopyrightText: 2022 Tobias Fella <tobias.fella@kde.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.components as KirigamiComponents
import org.kde.kirigamiaddons.delegates as Delegates

import org.kde.neochat
import org.kde.neochat.settings

RowLayout {
    id: root

    required property NeoChatConnection connection

    property bool collapsed: false

    property bool bottomEdge: true

    property var addAccount

    spacing: Kirigami.Units.largeSpacing

    Layout.topMargin: Kirigami.Units.smallSpacing
    Layout.bottomMargin: Kirigami.Units.smallSpacing
    Layout.rightMargin: Kirigami.Units.largeSpacing
    Layout.minimumHeight: bottomEdge ? Kirigami.Units.gridUnit * 3 : -1

    onVisibleChanged: {
        if (!visible) {
            accountsPopup.close();
        }
    }
    KirigamiComponents.AvatarButton {
        id: accountButton
        readonly property url avatarUrl: root.connection.localUser.avatarUrl

        Layout.preferredWidth: Kirigami.Units.iconSizes.medium
        Layout.preferredHeight: Kirigami.Units.iconSizes.medium
        Layout.leftMargin: Kirigami.Units.largeSpacing

        text: i18n("Edit this account")
        // Note: User::avatarUrl does not set user_id, and thus cannot be used directly here. Hence the makeMediaUrl.
        source: avatarUrl.toString().length > 0 ? root.connection.makeMediaUrl(avatarUrl) : ""
        name: root.connection.localUser.displayName

        activeFocusOnTab: true

        onClicked: {
            NeoChatSettingsView.open("accounts")
        }

        onPressAndHold: accountMenu.popup();

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: accountMenu.popup()
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.maximumWidth: Math.round(root.width * 0.55)
        visible: !root.collapsed
        spacing: 0
        QQC2.Label {
            id: displayNameLabel
            Layout.fillWidth: true
            text: root.connection.localUser.displayName
            textFormat: Text.PlainText
            elide: Text.ElideRight
        }
        QQC2.Label {
            id: idLabel
            Layout.fillWidth: true
            text: (root.connection.label.length > 0 ? (root.connection.label + " ") : "") + root.connection.localUser.id
            font.pointSize: displayNameLabel.font.pointSize * 0.8
            opacity: 0.7
            textFormat: Text.PlainText
            elide: Text.ElideRight
        }
    }
    Kirigami.ActionToolBar {
        alignment: Qt.AlignRight
        display: QQC2.Button.IconOnly

        actions: [
            Kirigami.Action {
                text: i18n("Switch User")
                icon.name: "system-switch-user"
                shortcut: "Ctrl+U"
                onTriggered: accountSwitchDialog.createObject(QQC2.Overlay.overlay, {
                    connection: root.connection
                }).open();
            },
            Kirigami.Action {
                text: i18n("Open Settings")
                icon.name: "settings-configure-symbolic"
                onTriggered: {
                    NeoChatSettingsView.open();
                }
            }
        ]
    }

    AccountMenu {
        id: accountMenu
        y: root.bottomEdge ? -height : accountButton.height
        connection: root.connection
        window: accountButton.QQC2.ApplicationWindow.window as Kirigami.ApplicationWindow
    }
    Component {
        id: accountSwitchDialog
        AccountSwitchDialog {}
    }
}

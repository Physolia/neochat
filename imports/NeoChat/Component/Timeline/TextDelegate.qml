// SPDX-FileCopyrightText: 2020 Black Hat <bhat@encom.eu.org>
// SPDX-License-Identifier: GPL-3.0-only

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15

import org.kde.neochat 1.0
import org.kde.kirigami 2.15 as Kirigami

TextEdit {
    id: contentLabel

    Layout.margins: Kirigami.Units.largeSpacing
    Layout.topMargin: 0

    property bool isSlashMe: false
    property string textMessage: model.display

    Component.onCompleted: EmojiFormatter.attach(contentLabel.textDocument)

    text: textMessage + (isEdited ? i18n(" (edited)") : "")

    color: Kirigami.Theme.textColor
    selectByMouse: !Kirigami.Settings.isMobile
    readOnly: true
    wrapMode: Text.WordWrap
    textFormat: Text.RichText

    Layout.fillWidth: true

    onLinkActivated: RoomManager.openResource(link)
    onHoveredLinkChanged: if (hoveredLink.length > 0) {
        applicationWindow().hoverLinkIndicator.text = hoveredLink;
    } else {
        applicationWindow().hoverLinkIndicator.text = "";
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.IBeamCursor
    }
}

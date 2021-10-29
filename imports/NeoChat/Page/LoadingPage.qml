// SPDX-FileCopyrightText: 2020 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.12 as QQC2
import org.kde.kirigami 2.12 as Kirigami
import org.kde.neochat 1.0

Kirigami.Page {
    title: i18n("Loading")

    Kirigami.PlaceholderMessage {
        id: loadingIndicator
        anchors.centerIn: parent
        text: i18n("Loading")
        QQC2.BusyIndicator {
            running: loadingIndicator.visible
            Layout.alignment: Qt.AlignHCenter
        }
    }

    footer: QQC2.Control {
        contentItem: Kirigami.InlineMessage {
            visible: !Config.ignoreThemeWarning && Controller.iconTheme !== 'breeze' && Controller.iconTheme !== 'breeze-dark' && Controller.iconTheme !== 'oxygen' && Controller.iconTheme !== 'Breeze'
            type: Kirigami.MessageType.Warning
            text: i18n("Your icon theme is not supported. NeoChat has been only tested with the Breeze and Oxygen icon theme. Some icon in the UI might be missing.")
            showCloseButton: true
            onVisibleChanged: if (!visible) {
                Config.ignoreThemeWarning = true;
                Config.save();
            }
        }
    }
}

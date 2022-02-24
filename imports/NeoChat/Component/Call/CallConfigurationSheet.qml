// SPDX-FileCopyrightText: 2021 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: LGPL-2.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2

import org.kde.kirigami 2.15 as Kirigami

import org.kde.neochat 1.0

Kirigami.OverlaySheet {
    id: sheet
    property var room: undefined

    title: i18n("Start call")

    Kirigami.FormLayout {
        QQC2.ComboBox {
            id: audioSource
            Kirigami.FormData.label: i18n("Audio Source:")
            model: AudioSources
            currentIndex: AudioSources.currentIndex
            onCurrentIndexChanged: AudioSources.currentIndex = currentIndex
        }
        QQC2.ComboBox {
            id: videoSource
            Kirigami.FormData.label: i18n("Video Source:")
            model: VideoSources
            currentIndex: VideoSources.currentIndex
            onCurrentIndexChanged: VideoSources.currentIndex = currentIndex
        }
        //QQC2.ComboBox {
            //id: caps
            //onModelChanged: currentIndex = VideoSources.capsIndex
            //enabled: sendVideo.checked
            //Kirigami.FormData.label: i18n("Framerate:")
        //}
    }
    footer: Kirigami.ActionToolBar {
        actions: [
            Kirigami.Action {
                text: i18n("Close")
                icon.name: "window-close-symbolic"
            }
        ]
    }
}

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
        }
        QQC2.CheckBox {
            id: sendVideo
            text: i18n("Send camera")
        }
        QQC2.ComboBox {
            id: videoSource
            Kirigami.FormData.label: i18n("Video Source:")
            model: VideoSources
            enabled: sendVideo.checked
            currentIndex: VideoSources.currentIndex
            onCountChanged: caps.model = VideoSources.caps(videoSource.currentIndex)
            onCurrentIndexChanged: caps.model = VideoSources.caps(videoSource.currentIndex)
        }
        QQC2.ComboBox {
            id: caps
            onModelChanged: currentIndex = VideoSources.capsIndex
            enabled: sendVideo.checked
            Kirigami.FormData.label: i18n("Framerate:")
        }
    }
    footer: Kirigami.ActionToolBar {
        actions: [
            Kirigami.Action {
                text: i18n("Call")
                icon.name: "call-start"
                onTriggered: {
                    if(sendVideo.checked) {
                        VideoSources.currentIndex = videoSource.currentIndex
                        VideoSources.capsIndex = caps.currentIndex
                    }
                    AudioSources.currentIndex = audioSource.currentIndex
                    CallManager.startCall(sheet.room, sendVideo.checked)
                    sheet.close()
                }
            }
        ]
    }
}

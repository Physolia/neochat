import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.0
import Qt.labs.platform 1.0 as Platform

import Spectral 0.1
import Spectral.Setting 0.1

import Spectral.Component 2.0
import Spectral.Dialog 2.0
import Spectral.Menu.Timeline 2.0
import Spectral.Effect 2.0

Image {
    readonly property bool isAnimated: contentType === "image/gif"

    property bool openOnFinished: false
    readonly property bool downloaded: progressInfo && progressInfo.completed

    readonly property bool isThumbnail: !(content.info.thumbnail_info == null || content.thumbnailMediaId == null)
    //    readonly property var info: isThumbnail ? content.info.thumbnail_info : content.info
    readonly property var info: content.info
    readonly property string mediaId: isThumbnail ? content.thumbnailMediaId : content.mediaId

    id: img

    source: "image://mxc/" + mediaId

    sourceSize.width: info.w
    sourceSize.height: info.h

    fillMode: Image.PreserveAspectCrop

    layer.enabled: true
    layer.effect: OpacityMask {
        maskSource: Rectangle {
            width: img.width
            height: img.height
            radius: 18
        }
    }

    Control {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8

        horizontalPadding: 8
        verticalPadding: 4

        contentItem: RowLayout {
            Label {
                text: Qt.formatTime(time)
                color: "white"
                font.pixelSize: 12
            }

            Label {
                text: author.displayName
                color: "white"
                font.pixelSize: 12
            }
        }

        background: Rectangle {
            radius: height / 2
            color: "black"
            opacity: 0.3
        }
    }

    Rectangle {
        anchors.fill: parent

        visible: progressInfo.active && !downloaded

        color: "#BB000000"

        ProgressBar {
            anchors.centerIn: parent

            width: parent.width * 0.8

            from: 0
            to: progressInfo.total
            value: progressInfo.progress
        }
    }

    RippleEffect {
        anchors.fill: parent

        id: messageMouseArea

        onPrimaryClicked: fullScreenImage.createObject(parent, {"filename": eventId, "localPath": currentRoom.urlToDownload(eventId)}).showFullScreen()

        onSecondaryClicked: {
            var contextMenu = imageDelegateContextMenu.createObject(root)
            contextMenu.viewSource.connect(function() {
                messageSourceDialog.createObject(ApplicationWindow.overlay, {"sourceText": toolTip}).open()
            })
            contextMenu.downloadAndOpen.connect(downloadAndOpen)
            contextMenu.saveFileAs.connect(saveFileAs)
            contextMenu.reply.connect(function() {
                roomPanelInput.replyModel = Object.assign({}, model)
                roomPanelInput.isReply = true
                roomPanelInput.focus()
            })
            contextMenu.redact.connect(function() {
                currentRoom.redactEvent(eventId)
            })
            contextMenu.popup()
        }

        Component {
            id: messageSourceDialog

            MessageSourceDialog {}
        }

        Component {
            id: openFolderDialog

            OpenFolderDialog {}
        }

        Component {
            id: imageDelegateContextMenu

            FileDelegateContextMenu {}
        }

        Component {
            id: fullScreenImage

            FullScreenImage {}
        }
    }

    function saveFileAs() {
        var folderDialog = openFolderDialog.createObject(ApplicationWindow.overlay)

        folderDialog.chosen.connect(function(path) {
            if (!path) return

            currentRoom.downloadFile(eventId, path + "/" + currentRoom.fileNameToDownload(eventId))
        })

        folderDialog.open()
    }

    function downloadAndOpen()
    {
        if (downloaded) openSavedFile()
        else
        {
            openOnFinished = true
            currentRoom.downloadFile(eventId, Platform.StandardPaths.writableLocation(Platform.StandardPaths.CacheLocation) + "/" + eventId.replace(":", "_").replace("/", "_").replace("+", "_") + currentRoom.fileNameToDownload(eventId))
        }
    }

    function openSavedFile()
    {
        if (Qt.openUrlExternally(progressInfo.localPath)) return;
        if (Qt.openUrlExternally(progressInfo.localDir)) return;
    }
}

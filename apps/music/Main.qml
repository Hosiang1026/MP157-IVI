import QtQuick
import QtQuick.Controls
import Ivi.Services 1.0

Item {
    id: root

    function mmss(value) {
        const s = Math.max(0, Math.floor(value))
        const m = Math.floor(s / 60)
        const r = s % 60
        return m + ":" + (r < 10 ? "0" : "") + r
    }

    Rectangle {
        anchors.fill: parent
        color: SystemState.page
    }

    Row {
        anchors.fill: parent
        anchors.margins: 16
        anchors.bottomMargin: 8
        spacing: 16

        Rectangle {
            width: parent.width * 0.56
            height: parent.height
            radius: 12
            color: SystemState.card

            Column {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 14

                Text { text: "音乐"; color: SystemState.ink; font.pixelSize: 28; font.bold: true }
                Text { text: MediaSession.source; color: SystemState.secondary; font.pixelSize: 13 }

                Row {
                    spacing: 16
                    Rectangle {
                        width: 112
                        height: 112
                        radius: 12
                        color: "#FF2D55"
                        Text {
                            anchors.centerIn: parent
                            text: MediaSession.title.length > 0 ? MediaSession.title.charAt(0) : ""
                            color: "#FFFFFF"
                            font.pixelSize: 42
                            font.bold: true
                        }
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6
                        Text { text: MediaSession.title; color: SystemState.ink; font.pixelSize: 22; font.bold: true }
                        Text { text: MediaSession.artist; color: SystemState.secondary; font.pixelSize: 15 }
                        Text {
                            text: !MediaSession.playing ? "已暂停" : (AudioFocus.ducked ? "通话中，已压低" : "正在播放")
                            color: AudioFocus.ducked ? "#FF9500" : "#007AFF"
                            font.pixelSize: 13
                        }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 4
                    Slider {
                        id: seek
                        width: parent.width
                        from: 0
                        to: Math.max(1, MediaSession.duration)
                        onMoved: MediaSession.seek(Math.round(value))
                        Component.onCompleted: value = MediaSession.position
                        background: Rectangle {
                            x: seek.leftPadding
                            y: seek.topPadding + seek.availableHeight / 2 - height / 2
                            implicitHeight: 4
                            width: seek.availableWidth
                            height: 4
                            radius: 2
                            color: SystemState.fill
                            Rectangle {
                                width: seek.visualPosition * parent.width
                                height: parent.height
                                radius: 2
                                color: "#007AFF"
                            }
                        }
                        handle: Rectangle {
                            x: seek.leftPadding + seek.visualPosition * (seek.availableWidth - width)
                            y: seek.topPadding + seek.availableHeight / 2 - height / 2
                            width: 18
                            height: 18
                            radius: 9
                            color: "#FFFFFF"
                            border.color: "#D1D1D6"
                        }
                    }
                    Row {
                        width: parent.width
                        Text { text: root.mmss(MediaSession.position); color: SystemState.secondary; font.pixelSize: 12 }
                        Item { width: parent.width - 80; height: 1 }
                        Text { text: root.mmss(MediaSession.duration); color: SystemState.secondary; font.pixelSize: 12 }
                    }
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 22
                    Repeater {
                        model: [
                            { label: "上一首", action: "previous", fill: false },
                            { label: MediaSession.playing ? "暂停" : "播放", action: "toggle", fill: true },
                            { label: "下一首", action: "next", fill: false }
                        ]
                        delegate: Rectangle {
                            required property var modelData
                            width: 64
                            height: 64
                            radius: 32
                            color: modelData.fill ? "#007AFF" : "#E5E5EA"
                            Text {
                                anchors.centerIn: parent
                                text: modelData.label
                                color: modelData.fill ? "#FFFFFF" : "#007AFF"
                                font.pixelSize: 13
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (modelData.action === "previous")
                                        MediaSession.previous()
                                    else if (modelData.action === "next")
                                        MediaSession.next()
                                    else
                                        MediaSession.toggle()
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            width: parent.width * 0.44 - 16
            height: parent.height
            radius: 12
            color: SystemState.card
            Column {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8
                Text { text: "播放列表"; color: SystemState.ink; font.pixelSize: 20; font.bold: true }
                ListView {
                    width: parent.width
                    height: parent.parent.height - 48
                    clip: true
                    model: MediaSession.tracks
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        height: 56
                        color: index === MediaSession.trackIndex ? "#E5F1FF" : "transparent"
                        radius: 8
                        Column {
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text { text: modelData.title; color: SystemState.ink; font.pixelSize: 16 }
                            Text { text: modelData.artist; color: SystemState.secondary; font.pixelSize: 12 }
                        }
                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.mmss(modelData.duration)
                            color: SystemState.secondary
                            font.pixelSize: 12
                        }
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: SystemState.fill
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: MediaSession.playIndex(index)
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: MediaSession
        function onPositionChanged() {
            if (!seek.pressed)
                seek.value = MediaSession.position
        }
        function onTrackChanged() {
            seek.to = Math.max(1, MediaSession.duration)
            seek.value = MediaSession.position
        }
    }
}

import QtQuick
import Ivi.Services 1.0

Item {
    id: root
    property int index: 0
    property bool playing: false
    property int position: 0
    property var clips: [
        { title: "城市夜景" },
        { title: "沿海公路" },
        { title: "山路" },
        { title: "雨天" },
        { title: "日落" }
    ]

    function current() { return clips[index] }

    function mmss(value) {
        const s = Math.max(0, Math.floor(value))
        const m = Math.floor(s / 60)
        const r = s % 60
        return m + ":" + (r < 10 ? "0" : "") + r
    }

    function playIndex(i) {
        index = i
        screen.clipName = root.clips[i].title
        screen.seek(0)
        screen.playing = true
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
            width: parent.width * 0.62
            height: parent.height
            radius: 12
            color: SystemState.card
            Column {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 14
                Text { text: "视频"; color: SystemState.ink; font.pixelSize: 28; font.bold: true }
                VideoScreen {
                    id: screen
                    width: parent.width
                    height: 280
                    clipName: root.clips[root.index].title
                }
                Text { text: root.mmss(screen.position) + " / " + root.mmss(screen.duration); color: SystemState.secondary; font.pixelSize: 13 }
                Rectangle {
                    width: 72
                    height: 36
                    radius: 18
                    color: "#5E5CE6"
                    Text {
                        anchors.centerIn: parent
                        text: screen.playing ? "暂停" : "播放"
                        color: "#FFFFFF"
                        font.pixelSize: 14
                    }
                    MouseArea { anchors.fill: parent; onClicked: screen.playing = !screen.playing }
                }
            }
        }

        Rectangle {
            width: parent.width * 0.38 - 16
            height: parent.height
            radius: 12
            color: SystemState.card
            Column {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8
                Text { text: "片单"; color: SystemState.ink; font.pixelSize: 20; font.bold: true }
                Repeater {
                    model: root.clips
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: parent.width
                        height: 52
                        radius: 8
                        color: index === root.index ? SystemState.highlight : "transparent"
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.title
                            color: SystemState.ink
                            font.pixelSize: 16
                        }
                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.mmss(8)
                            color: SystemState.secondary
                            font.pixelSize: 13
                        }
                        MouseArea { anchors.fill: parent; onClicked: root.playIndex(index) }
                    }
                }
            }
        }
    }
}

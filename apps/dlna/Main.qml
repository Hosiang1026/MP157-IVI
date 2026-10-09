import QtQuick
import Ivi.Services 1.0

Item {
    id: root

    Rectangle {
        anchors.fill: parent
        color: "#000000"
    }

    DlnaVideoItem {
        anchors.fill: parent
        visible: DlnaRenderer.hasFrame
        z: 10
        session: DlnaRenderer
    }

    Column {
        anchors.centerIn: parent
        spacing: 14
        visible: !DlnaRenderer.hasFrame
        width: parent.width - 48
        z: 5

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "DLNA 投屏"
            color: "#FFFFFF"
            font.pixelSize: 28
            font.bold: true
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: DlnaRenderer.status
            color: "#FFFFFF"
            font.pixelSize: 20
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: DlnaRenderer.detail
            color: "#A1A1A6"
            font.pixelSize: 14
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            visible: DlnaRenderer.hasMedia
            text: DlnaRenderer.mediaTitle + " · " + DlnaRenderer.transportState
            color: "#A1A1A6"
            font.pixelSize: 13
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12
            Rectangle {
                width: 120
                height: 40
                radius: 20
                color: DlnaRenderer.running ? "#3A3A3C" : "#0A84FF"
                Text {
                    anchors.centerIn: parent
                    text: DlnaRenderer.running ? "运行中" : "开始接收"
                    color: "#FFFFFF"
                    font.pixelSize: 15
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: !DlnaRenderer.running
                    onClicked: DlnaRenderer.start()
                }
            }
            Rectangle {
                width: 100
                height: 40
                radius: 20
                color: "#FF453A"
                visible: DlnaRenderer.running
                Text {
                    anchors.centerIn: parent
                    text: "停止"
                    color: "#FFFFFF"
                    font.pixelSize: 15
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: DlnaRenderer.stop()
                }
            }
            Rectangle {
                width: 100
                height: 40
                radius: 20
                color: "#8E8E93"
                visible: DlnaRenderer.hasMedia
                Text {
                    anchors.centerIn: parent
                    text: "清除"
                    color: "#FFFFFF"
                    font.pixelSize: 15
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: DlnaRenderer.stopMedia()
                }
            }
        }
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        spacing: 12
        visible: DlnaRenderer.hasFrame
        z: 20

        Rectangle {
            width: 88
            height: 36
            radius: 18
            color: "#3A3A3C"
            Text {
                anchors.centerIn: parent
                text: DlnaRenderer.transportState === "PAUSED_PLAYBACK" ? "继续" : "暂停"
                color: "#FFFFFF"
                font.pixelSize: 14
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (DlnaRenderer.transportState === "PAUSED_PLAYBACK")
                        DlnaRenderer.resumeMedia()
                    else
                        DlnaRenderer.pauseMedia()
                }
            }
        }
        Rectangle {
            width: 88
            height: 36
            radius: 18
            color: "#FF453A"
            Text {
                anchors.centerIn: parent
                text: "停止"
                color: "#FFFFFF"
                font.pixelSize: 14
            }
            MouseArea {
                anchors.fill: parent
                onClicked: DlnaRenderer.stopMedia()
            }
        }
    }

    Component.onDestruction: {
        if (DlnaRenderer.running)
            DlnaRenderer.stop()
    }
}

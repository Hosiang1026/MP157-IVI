import QtQuick
import Ivi.Services 1.0
import IviShell

Item {
    id: root

    readonly property bool connecting: RadioSession.status.indexOf("连接") >= 0
    readonly property var accentColors: ["#FF2D55", "#5856D6", "#007AFF", "#34C759", "#FF9500", "#AF52DE"]

    Rectangle {
        anchors.fill: parent
        color: "transparent"
    }

    Row {
        anchors.fill: parent
        anchors.margins: 16
        anchors.bottomMargin: 8
        spacing: 16

        Rectangle {
            width: 360
            height: parent.height
            radius: 16
            color: SystemState.card
            border.width: 1 / Screen.devicePixelRatio
            border.color: SystemState.separator

            Column {
                anchors.centerIn: parent
                spacing: 18
                width: parent.width - 40

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "网络电台"
                    color: SystemState.secondary
                    font.pixelSize: 14
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: RadioSession.stationName.length ? RadioSession.stationName : "选择电台"
                    color: SystemState.ink
                    font.pixelSize: 28
                    font.bold: true
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 8
                    IosSpinner {
                        width: 16
                        height: 16
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.connecting
                        running: root.connecting
                        ink: SystemState.tint
                    }
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: RadioSession.playing ? SystemState.success : SystemState.tint
                        visible: RadioSession.status.length > 0 && !root.connecting
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: RadioSession.status
                        color: RadioSession.playing ? SystemState.success : SystemState.secondary
                        font.pixelSize: 16
                    }
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: RadioSession.detail
                    color: SystemState.secondary
                    font.pixelSize: 12
                    visible: RadioSession.detail.length > 0
                }

                IosPressable {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 88
                    height: 88
                    Rectangle {
                        anchors.fill: parent
                        radius: 44
                        color: RadioSession.playing ? SystemState.danger : SystemState.tint
                    }
                    IosIcon {
                        anchors.centerIn: parent
                        width: 32
                        height: 32
                        name: RadioSession.playing ? "pause" : "play"
                        ink: "#FFFFFF"
                    }
                    onClicked: RadioSession.toggle()
                }
            }
        }

        Rectangle {
            width: parent.width - 376
            height: parent.height
            radius: 16
            color: SystemState.card
            border.width: 1 / Screen.devicePixelRatio
            border.color: SystemState.separator
            clip: true

            Column {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10
                visible: RadioSession.stations.length > 0

                Text {
                    text: "电台列表"
                    color: SystemState.ink
                    font.pixelSize: 18
                    font.bold: true
                }

                ListView {
                    width: parent.width
                    height: parent.height - 36
                    clip: true
                    model: RadioSession.stations
                    spacing: 0
                    delegate: IosPressable {
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        height: 64

                        Rectangle {
                            anchors.fill: parent
                            color: RadioSession.currentIndex === index ? SystemState.selected : "transparent"
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: 3
                            height: 28
                            radius: 1.5
                            color: root.accentColors[index % root.accentColors.length]
                            visible: RadioSession.currentIndex === index
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            width: 36
                            height: 36
                            radius: 18
                            color: root.accentColors[index % root.accentColors.length]
                            Text {
                                anchors.centerIn: parent
                                text: modelData.name.length ? modelData.name.charAt(0) : "?"
                                color: "#FFFFFF"
                                font.pixelSize: 15
                                font.bold: true
                            }
                        }

                        Column {
                            anchors.left: parent.left
                            anchors.leftMargin: 64
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 4
                            Text {
                                text: modelData.name
                                color: RadioSession.currentIndex === index ? SystemState.tint : SystemState.ink
                                font.pixelSize: 17
                                font.bold: RadioSession.currentIndex === index
                            }
                            Text {
                                text: modelData.genre
                                color: SystemState.secondary
                                font.pixelSize: 12
                            }
                        }

                        Rectangle {
                            anchors.right: parent.right
                            anchors.rightMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            width: 8
                            height: 8
                            radius: 4
                            visible: RadioSession.playing && RadioSession.currentIndex === index
                            color: SystemState.success
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.leftMargin: 64
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 0.5
                            color: SystemState.separator
                            visible: index < RadioSession.stations.length - 1
                        }

                        onClicked: RadioSession.playIndex(index)
                    }
                }
            }

            IosEmptyState {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: RadioSession.stations.length === 0
                icon: "speaker"
                title: "暂无电台"
                subtitle: "稍后再试或检查网络"
            }
        }
    }

    Component.onDestruction: {
        if (RadioSession.playing)
            RadioSession.stop()
    }
}

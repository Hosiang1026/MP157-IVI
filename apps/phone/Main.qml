import QtQuick
import Ivi.Services 1.0

Item {
    id: root
    property string input: ""
    property string tab: "contacts"

    function clock(value) {
        const s = Math.max(0, value)
        const m = Math.floor(s / 60)
        const r = s % 60
        return (m < 10 ? "0" : "") + m + ":" + (r < 10 ? "0" : "") + r
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
            width: 400
            height: parent.height
            radius: 12
            color: SystemState.card

            Column {
                anchors.centerIn: parent
                spacing: 14
                visible: !CallSession.active

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.input.length > 0 ? root.input : "键入号码"
                    color: root.input.length > 0 ? "#000000" : "#C7C7CC"
                    font.pixelSize: 28
                }

                Grid {
                    columns: 3
                    rowSpacing: 10
                    columnSpacing: 16
                    Repeater {
                        model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"]
                        delegate: Rectangle {
                            required property string modelData
                            width: 64
                            height: 64
                            radius: 32
                            color: SystemState.fill
                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: SystemState.ink
                                font.pixelSize: 24
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (root.input.length < 16)
                                        root.input += modelData
                                }
                            }
                        }
                    }
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 28
                    Rectangle {
                        width: 64
                        height: 64
                        radius: 32
                        color: SystemState.fill
                        Text { anchors.centerIn: parent; text: "删除"; color: "#007AFF"; font.pixelSize: 13 }
                        MouseArea { anchors.fill: parent; onClicked: root.input = root.input.slice(0, -1) }
                    }
                    Rectangle {
                        width: 64
                        height: 64
                        radius: 32
                        color: "#34C759"
                        Text { anchors.centerIn: parent; text: "呼叫"; color: "#FFFFFF"; font.pixelSize: 14 }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (root.input.length > 0)
                                    CallSession.dialNumber(root.input)
                            }
                        }
                    }
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 16
                visible: CallSession.active
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: CallSession.contactName.length > 0 ? CallSession.contactName : "未知号码"
                    color: SystemState.ink
                    font.pixelSize: 28
                    font.bold: true
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: CallSession.number
                    color: SystemState.secondary
                    font.pixelSize: 16
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.clock(CallSession.elapsed)
                    color: "#34C759"
                    font.pixelSize: 20
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 18
                    Rectangle {
                        width: 72
                        height: 72
                        radius: 36
                        color: CallSession.muted ? "#FF9500" : "#E5E5EA"
                        Text { anchors.centerIn: parent; text: "静音"; color: CallSession.muted ? "#FFFFFF" : "#000000"; font.pixelSize: 14 }
                        MouseArea { anchors.fill: parent; onClicked: CallSession.toggleMuted() }
                    }
                    Rectangle {
                        width: 72
                        height: 72
                        radius: 36
                        color: CallSession.speaker ? "#007AFF" : "#E5E5EA"
                        Text { anchors.centerIn: parent; text: "免提"; color: CallSession.speaker ? "#FFFFFF" : "#000000"; font.pixelSize: 14 }
                        MouseArea { anchors.fill: parent; onClicked: CallSession.toggleSpeaker() }
                    }
                    Rectangle {
                        width: 72
                        height: 72
                        radius: 36
                        color: "#FF3B30"
                        Text { anchors.centerIn: parent; text: "挂断"; color: "#FFFFFF"; font.pixelSize: 14 }
                        MouseArea { anchors.fill: parent; onClicked: CallSession.hangup() }
                    }
                }
            }
        }

        Rectangle {
            width: parent.width - 416
            height: parent.height
            radius: 12
            color: SystemState.card

            Column {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Rectangle {
                    width: 176
                    height: 32
                    radius: 8
                    color: SystemState.fill
                    Row {
                        anchors.fill: parent
                        anchors.margins: 2
                        Repeater {
                            model: [
                                { id: "contacts", label: "联系人" },
                                { id: "recents", label: "最近" }
                            ]
                            delegate: Rectangle {
                                required property var modelData
                                width: 86
                                height: 28
                                radius: 7
                                color: root.tab === modelData.id ? SystemState.card : "transparent"
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: SystemState.ink
                                    font.pixelSize: 13
                                    font.bold: root.tab === modelData.id
                                }
                                MouseArea { anchors.fill: parent; onClicked: root.tab = modelData.id }
                            }
                        }
                    }
                }

                ListView {
                    width: parent.width
                    height: parent.height - 56
                    clip: true
                    model: root.tab === "contacts" ? CallSession.contacts : CallSession.recents
                    delegate: Item {
                        required property var modelData
                        width: ListView.view.width
                        height: 52
                        Column {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text { text: modelData.name; color: SystemState.ink; font.pixelSize: 16 }
                            Text { text: modelData.number; color: SystemState.secondary; font.pixelSize: 12 }
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
                            onClicked: {
                                if (CallSession.active)
                                    return
                                root.input = modelData.number
                                CallSession.dialNumber(modelData.number)
                            }
                        }
                    }
                }

                Text {
                    visible: root.tab === "recents" && CallSession.recents.length === 0
                    text: "没有最近通话"
                    color: SystemState.secondary
                    font.pixelSize: 14
                }
            }
        }
    }
}

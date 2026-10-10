import QtQuick
import Ivi.Services 1.0
import IviShell

Item {
    id: root
    property string input: ""
    property string tab: "keypad"
    property string contactQuery: ""
    readonly property bool inCall: CallSession.active || CallSession.ringing
    readonly property var filteredContacts: {
        const q = contactQuery.trim().toLowerCase()
        const all = CallSession.contacts
        if (q.length === 0)
            return all
        const out = []
        for (let i = 0; i < all.length; ++i) {
            const c = all[i]
            if (String(c.name).toLowerCase().indexOf(q) >= 0 || String(c.number).indexOf(q) >= 0)
                out.push(c)
        }
        return out
    }
    readonly property var dialKeys: [
        { d: "1", s: "" },
        { d: "2", s: "ABC" },
        { d: "3", s: "DEF" },
        { d: "4", s: "GHI" },
        { d: "5", s: "JKL" },
        { d: "6", s: "MNO" },
        { d: "7", s: "PQRS" },
        { d: "8", s: "TUV" },
        { d: "9", s: "WXYZ" },
        { d: "*", s: "" },
        { d: "0", s: "+" },
        { d: "#", s: "" }
    ]

    function clock(value) {
        const s = Math.max(0, value)
        const m = Math.floor(s / 60)
        const r = s % 60
        return (m < 10 ? "0" : "") + m + ":" + (r < 10 ? "0" : "") + r
    }

    function carPlayPulse(key) {
        if (!CarPlaySession.running)
            return false
        CarPlaySession.sendHardKey(key, true)
        CarPlaySession.sendHardKey(key, false)
        return true
    }

    function acceptCall() {
        if (root.carPlayPulse("phone_accept"))
            return
        CallSession.answer()
    }

    function endCall() {
        if (CallSession.ringing)
            root.carPlayPulse("phone_reject")
        else
            root.carPlayPulse("phone_end")
        CallSession.hangup()
    }

    function dialFromList(number) {
        if (root.inCall)
            return
        root.input = number
        CallSession.dialNumber(number)
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
    }

    Item {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: tabBar.top
        visible: !root.inCall

        Item {
            anchors.fill: parent
            visible: root.tab === "keypad"

            Column {
                anchors.centerIn: parent
                spacing: 18
                width: Math.min(parent.width - 32, 320)

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: root.input.length > 0 ? root.input : " "
                    color: root.input.length > 0 ? SystemState.ink : SystemState.secondary
                    font.pixelSize: 36
                    font.letterSpacing: 1
                    elide: Text.ElideLeft
                }

                Grid {
                    anchors.horizontalCenter: parent.horizontalCenter
                    columns: 3
                    rowSpacing: 12
                    columnSpacing: 22
                    Repeater {
                        model: root.dialKeys
                        delegate: IosPressable {
                            required property var modelData
                            width: 72
                            height: 72
                            onClicked: {
                                if (root.input.length < 16)
                                    root.input += modelData.d
                            }
                            Rectangle {
                                anchors.fill: parent
                                radius: 36
                                color: SystemState.fill
                                Column {
                                    anchors.centerIn: parent
                                    spacing: 0
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.d
                                        color: SystemState.ink
                                        font.pixelSize: 28
                                    }
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.s.length > 0 ? modelData.s : " "
                                        color: SystemState.secondary
                                        font.pixelSize: 10
                                        font.letterSpacing: 1
                                        opacity: modelData.s.length > 0 ? 1 : 0
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: 72
                    IosPressable {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: callBtn.left
                        anchors.rightMargin: 36
                        width: 56
                        height: 56
                        visible: root.input.length > 0
                        onClicked: root.input = root.input.slice(0, -1)
                        IosIcon {
                            anchors.centerIn: parent
                            width: 28
                            height: 28
                            name: "delete"
                            ink: SystemState.tint
                        }
                    }
                    IosPressable {
                        id: callBtn
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        width: 72
                        height: 72
                        onClicked: {
                            if (root.input.length > 0)
                                CallSession.dialNumber(root.input)
                        }
                        Rectangle {
                            anchors.fill: parent
                            radius: 36
                            color: SystemState.success
                            IosIcon {
                                anchors.centerIn: parent
                                width: 30
                                height: 30
                                name: "phone"
                                ink: "#FFFFFF"
                            }
                        }
                    }
                }
            }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10
            visible: root.tab === "recents"

            Text {
                text: "最近"
                color: SystemState.ink
                font.pixelSize: 28
                font.bold: true
            }

            Item {
                width: parent.width
                height: parent.height - 48

                ListView {
                    anchors.fill: parent
                    clip: true
                    visible: CallSession.recents.length > 0
                    model: CallSession.recents
                    delegate: Item {
                        required property var modelData
                        width: ListView.view.width
                        height: 52
                        IosPressable {
                            anchors.fill: parent
                            onClicked: root.dialFromList(modelData.number)
                            Item {
                                anchors.fill: parent
                                Column {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2
                                    Text { text: modelData.name; color: SystemState.ink; font.pixelSize: 16 }
                                    Text { text: modelData.number; color: SystemState.secondary; font.pixelSize: 12 }
                                }
                                IosIcon {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 14
                                    height: 14
                                    name: "chevron"
                                    ink: SystemState.secondary
                                }
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 0.5
                                    color: SystemState.separator
                                }
                            }
                        }
                    }
                }

                IosEmptyState {
                    anchors.centerIn: parent
                    width: parent.width - 32
                    visible: CallSession.recents.length === 0
                    icon: "clock"
                    title: "暂无最近通话"
                    subtitle: "拨出的电话会出现在这里"
                }
            }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10
            visible: root.tab === "contacts"

            Text {
                text: "联系人"
                color: SystemState.ink
                font.pixelSize: 28
                font.bold: true
            }

            IosSearchField {
                id: contactSearch
                width: parent.width
                placeholder: "搜索联系人"
                onTextChanged: root.contactQuery = text
                onCleared: root.contactQuery = ""
            }

            Item {
                width: parent.width
                height: parent.height - 96

                ListView {
                    anchors.fill: parent
                    clip: true
                    visible: root.filteredContacts.length > 0
                    model: root.filteredContacts
                    delegate: Item {
                        required property var modelData
                        width: ListView.view.width
                        height: 52
                        IosPressable {
                            anchors.fill: parent
                            onClicked: root.dialFromList(modelData.number)
                            Item {
                                anchors.fill: parent
                                Column {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2
                                    Text { text: modelData.name; color: SystemState.ink; font.pixelSize: 16 }
                                    Text { text: modelData.number; color: SystemState.secondary; font.pixelSize: 12 }
                                }
                                IosIcon {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 14
                                    height: 14
                                    name: "chevron"
                                    ink: SystemState.secondary
                                }
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 0.5
                                    color: SystemState.separator
                                }
                            }
                        }
                    }
                }

                IosEmptyState {
                    anchors.centerIn: parent
                    width: parent.width - 32
                    visible: root.filteredContacts.length === 0
                    icon: "person"
                    title: "暂无联系人"
                    subtitle: root.contactQuery.length > 0 ? "没有匹配的联系人" : "联系人列表为空"
                }
            }
        }
    }

    Rectangle {
        anchors.fill: content
        color: SystemState.card
        border.width: 1 / Screen.devicePixelRatio
        border.color: SystemState.separator
        visible: root.inCall
        radius: 0

        Column {
            anchors.centerIn: parent
            spacing: 20
            width: parent.width - 48

            Item {
                width: 88
                height: 88
                anchors.horizontalCenter: parent.horizontalCenter
                Rectangle {
                    id: ringPulse
                    anchors.centerIn: parent
                    width: 72
                    height: 72
                    radius: 36
                    color: "transparent"
                    border.color: CallSession.ringing ? SystemState.warning : SystemState.success
                    border.width: 2
                    opacity: CallSession.ringing ? 0.85 : 0.35
                    SequentialAnimation on scale {
                        loops: Animation.Infinite
                        running: CallSession.ringing && ringPulse.visible
                        NumberAnimation { from: 1; to: 1.18; duration: 700; easing.type: Easing.OutQuad }
                        NumberAnimation { from: 1.18; to: 1; duration: 700; easing.type: Easing.InQuad }
                    }
                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        running: CallSession.ringing && ringPulse.visible
                        NumberAnimation { from: 0.9; to: 0.2; duration: 700 }
                        NumberAnimation { from: 0.2; to: 0.9; duration: 700 }
                    }
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 64
                    height: 64
                    radius: 32
                    color: CallSession.ringing ? SystemState.warning : SystemState.success
                    IosIcon {
                        anchors.centerIn: parent
                        width: 28
                        height: 28
                        name: "phone"
                        ink: "#FFFFFF"
                    }
                }
                Rectangle {
                    visible: CarPlaySession.running
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: 2
                    anchors.bottomMargin: 2
                    width: 28
                    height: 16
                    radius: 4
                    color: SystemState.tint
                    Text {
                        anchors.centerIn: parent
                        text: "CP"
                        color: "#FFFFFF"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: CallSession.contactName.length > 0 ? CallSession.contactName : "未知号码"
                color: SystemState.ink
                font.pixelSize: 30
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
                text: CallSession.ringing
                      ? (CarPlaySession.running ? "来电" : "正在呼叫")
                      : root.clock(CallSession.elapsed)
                color: CallSession.ringing ? SystemState.warning : SystemState.success
                font.pixelSize: 20
                font.bold: !CallSession.ringing
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 28

                IosPressable {
                    width: 72
                    height: 72
                    visible: CallSession.ringing
                    onClicked: root.acceptCall()
                    Rectangle {
                        anchors.fill: parent
                        radius: 36
                        color: SystemState.success
                        IosIcon {
                            anchors.centerIn: parent
                            width: 30
                            height: 30
                            name: "phone"
                            ink: "#FFFFFF"
                        }
                    }
                }

                IosPressable {
                    width: 72
                    height: 72
                    visible: CallSession.active
                    onClicked: CallSession.toggleMuted()
                    Rectangle {
                        anchors.fill: parent
                        radius: 36
                        color: CallSession.muted ? SystemState.selected : SystemState.fill
                        IosIcon {
                            anchors.centerIn: parent
                            width: 28
                            height: 28
                            name: "mic"
                            ink: CallSession.muted ? SystemState.tint : SystemState.ink
                        }
                    }
                }

                IosPressable {
                    width: 72
                    height: 72
                    visible: CallSession.active
                    onClicked: CallSession.toggleSpeaker()
                    Rectangle {
                        anchors.fill: parent
                        radius: 36
                        color: CallSession.speaker ? SystemState.selected : SystemState.fill
                        IosIcon {
                            anchors.centerIn: parent
                            width: 28
                            height: 28
                            name: "speaker"
                            ink: CallSession.speaker ? SystemState.tint : SystemState.ink
                        }
                    }
                }

                IosPressable {
                    width: 72
                    height: 72
                    onClicked: root.endCall()
                    Rectangle {
                        anchors.fill: parent
                        radius: 36
                        color: SystemState.danger
                        IosIcon {
                            anchors.centerIn: parent
                            width: 30
                            height: 30
                            name: "phoneDown"
                            ink: "#FFFFFF"
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: tabBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 56
        color: SystemState.card
        border.width: 1 / Screen.devicePixelRatio
        border.color: SystemState.separator
        visible: !root.inCall

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 0.5
            color: SystemState.separator
        }

        Row {
            anchors.fill: parent
            Repeater {
                model: [
                    { id: "keypad", icon: "keypad", label: "键盘" },
                    { id: "recents", icon: "clock", label: "最近" },
                    { id: "contacts", icon: "person", label: "联系人" }
                ]
                delegate: IosPressable {
                    required property var modelData
                    width: tabBar.width / 3
                    height: tabBar.height
                    onClicked: root.tab = modelData.id
                    Column {
                        anchors.centerIn: parent
                        spacing: 2
                        IosIcon {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 22
                            height: 22
                            name: modelData.icon
                            ink: root.tab === modelData.id ? SystemState.tint : SystemState.secondary
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.label
                            color: root.tab === modelData.id ? SystemState.tint : SystemState.secondary
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }
    }
}

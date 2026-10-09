import QtQuick
import QtQuick.Controls
import Ivi.Services 1.0

Item {
    id: root
    property string band: "FM"
    property real freq: 91.4
    property string station: "音乐之声"
    property bool on: false
    property var fmPresets: [
        { freq: 87.6, name: "交通广播" },
        { freq: 91.4, name: "音乐之声" },
        { freq: 97.4, name: "新闻综合" },
        { freq: 101.7, name: "经济之声" },
        { freq: 103.9, name: "文艺广播" },
        { freq: 107.3, name: "城市之声" }
    ]
    property var amPresets: [
        { freq: 540, name: "新闻" },
        { freq: 720, name: "交通" },
        { freq: 900, name: "经济" },
        { freq: 1035, name: "文艺" },
        { freq: 1260, name: "综合" },
        { freq: 1521, name: "音乐" }
    ]

    function presets() { return band === "FM" ? fmPresets : amPresets }

    function powerOn() {
        if (on)
            return
        on = true
        if (MediaSession.playing)
            MediaSession.pause()
        AudioFocus.request("radio", AudioFocus.mediaPriority)
        saveState()
    }

    function powerOff() {
        if (!on)
            return
        on = false
        AudioFocus.release("radio")
        saveState()
    }

    function tune(dir) {
        powerOn()
        station = "手动调谐"
        if (band === "FM") {
            freq = Math.round((freq + dir * 0.1) * 10) / 10
            if (freq > 108)
                freq = 87.5
            if (freq < 87.5)
                freq = 108
        } else {
            freq += dir * 9
            if (freq > 1602)
                freq = 531
            if (freq < 531)
                freq = 1602
        }
        saveState()
    }

    function usePreset(item) {
        powerOn()
        freq = item.freq
        station = item.name
        saveState()
    }

    function scan() {
        const list = presets()
        let next = 0
        for (let i = 0; i < list.length; ++i) {
            if (Math.abs(list[i].freq - freq) < 0.05) {
                next = (i + 1) % list.length
                break
            }
        }
        usePreset(list[next])
    }

    function saveCurrentPreset() {
        const list = band === "FM" ? fmPresets.slice() : amPresets.slice()
        let found = false
        for (let i = 0; i < list.length; ++i) {
            if (Math.abs(list[i].freq - freq) < 0.05) {
                list[i] = { freq: freq, name: station.length ? station : "自定义" }
                found = true
                break
            }
        }
        if (!found) {
            if (list.length >= 8)
                list.pop()
            list.unshift({ freq: freq, name: station.length ? station : "自定义" })
        }
        if (band === "FM")
            fmPresets = list
        else
            amPresets = list
        saveState()
    }

    function saveState() {
        SystemState.setPref("radio/band", band)
        SystemState.setPref("radio/freq", freq)
        SystemState.setPref("radio/station", station)
        SystemState.setPref("radio/fmJson", JSON.stringify(fmPresets))
        SystemState.setPref("radio/amJson", JSON.stringify(amPresets))
    }

    function loadState() {
        band = SystemState.pref("radio/band", "FM")
        freq = Number(SystemState.pref("radio/freq", 91.4))
        station = SystemState.pref("radio/station", "音乐之声")
        try {
            const fmJson = SystemState.pref("radio/fmJson", "")
            if (fmJson.length) {
                const fm = JSON.parse(fmJson)
                if (fm && fm.length)
                    fmPresets = fm
            }
            const amJson = SystemState.pref("radio/amJson", "")
            if (amJson.length) {
                const am = JSON.parse(amJson)
                if (am && am.length)
                    amPresets = am
            }
        } catch (e) {}
    }

    Component.onCompleted: loadState()
    Component.onDestruction: {
        AudioFocus.release("radio")
        saveState()
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
            width: Math.min(460, parent.width * 0.48)
            height: parent.height
            radius: 12
            color: SystemState.card
            Column {
                anchors.centerIn: parent
                spacing: 12
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 120
                    height: 32
                    radius: 8
                    color: SystemState.fill
                    Row {
                        anchors.fill: parent
                        anchors.margins: 2
                        Repeater {
                            model: ["FM", "AM"]
                            delegate: Rectangle {
                                required property string modelData
                                width: 58
                                height: 28
                                radius: 7
                                color: root.band === modelData ? SystemState.card : "transparent"
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: SystemState.ink
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        root.band = modelData
                                        root.usePreset(root.presets()[0])
                                    }
                                }
                            }
                        }
                    }
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.station
                    color: SystemState.secondary
                    font.pixelSize: 16
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.band === "FM" ? root.freq.toFixed(1) : Math.round(root.freq)
                    color: SystemState.ink
                    font.pixelSize: 64
                    font.bold: true
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.band === "FM" ? "MHz" : "kHz"
                    color: SystemState.secondary
                    font.pixelSize: 14
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.on ? "正在播放（模拟）" : "已关闭"
                    color: root.on ? "#007AFF" : "#8E8E93"
                    font.pixelSize: 14
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 12
                    Rectangle {
                        width: 56
                        height: 56
                        radius: 28
                        color: SystemState.fill
                        Text { anchors.centerIn: parent; text: "－"; color: "#007AFF"; font.pixelSize: 24 }
                        MouseArea { anchors.fill: parent; onClicked: root.tune(-1) }
                    }
                    Rectangle {
                        width: 72
                        height: 56
                        radius: 28
                        color: root.on ? "#007AFF" : "#E5E5EA"
                        Text {
                            anchors.centerIn: parent
                            text: root.on ? "关闭" : "播放"
                            color: root.on ? "#FFFFFF" : "#007AFF"
                            font.pixelSize: 15
                        }
                        MouseArea { anchors.fill: parent; onClicked: root.on ? root.powerOff() : root.powerOn() }
                    }
                    Rectangle {
                        width: 56
                        height: 56
                        radius: 28
                        color: SystemState.fill
                        Text { anchors.centerIn: parent; text: "＋"; color: "#007AFF"; font.pixelSize: 24 }
                        MouseArea { anchors.fill: parent; onClicked: root.tune(1) }
                    }
                    Rectangle {
                        width: 64
                        height: 56
                        radius: 28
                        color: SystemState.fill
                        Text { anchors.centerIn: parent; text: "搜台"; color: "#007AFF"; font.pixelSize: 14 }
                        MouseArea { anchors.fill: parent; onClicked: root.scan() }
                    }
                }
                Row {
                    spacing: 8
                    Text { text: "音量"; color: SystemState.secondary; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                    Slider {
                        id: volumeSlider
                        width: 240
                        from: 0
                        to: 1
                        Component.onCompleted: value = SystemState.volume
                        onMoved: SystemState.volume = value
                    }
                }
            }
        }

        Rectangle {
            width: parent.width - Math.min(460, parent.width * 0.48) - 16
            height: parent.height
            radius: 12
            color: SystemState.card
            Column {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8
                Row {
                    width: parent.width
                    Text {
                        text: "预设"
                        color: SystemState.ink
                        font.pixelSize: 20
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Item { width: Math.max(8, parent.width - 160); height: 1 }
                    Rectangle {
                        width: 88
                        height: 30
                        radius: 8
                        color: "#007AFF"
                        Text { anchors.centerIn: parent; text: "存为预设"; color: "#FFF"; font.pixelSize: 13 }
                        MouseArea { anchors.fill: parent; onClicked: root.saveCurrentPreset() }
                    }
                }
                Grid {
                    id: presetGrid
                    width: parent.width
                    columns: 2
                    rowSpacing: 8
                    columnSpacing: 8
                    Repeater {
                        model: root.presets()
                        delegate: Rectangle {
                            required property var modelData
                            width: (presetGrid.width - 8) / 2
                            height: 58
                            radius: 10
                            color: Math.abs(modelData.freq - root.freq) < 0.05 ? SystemState.highlight : SystemState.fill
                            Column {
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                Text { text: modelData.name; color: SystemState.ink; font.pixelSize: 15 }
                                Text {
                                    text: root.band === "FM" ? modelData.freq.toFixed(1) : modelData.freq
                                    color: "#007AFF"
                                    font.pixelSize: 12
                                }
                            }
                            MouseArea { anchors.fill: parent; onClicked: root.usePreset(modelData) }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: SystemState
        function onVolumeChanged() {
            if (!volumeSlider.pressed)
                volumeSlider.value = SystemState.volume
        }
    }
}

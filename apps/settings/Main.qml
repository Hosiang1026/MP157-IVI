import QtQuick
import QtQuick.Controls
import Ivi.Services 1.0
import IviShell

Item {
    id: root
    property bool wallPickerOpen: false
    property var wallPickList: []

    function openWallPicker() {
        wallPickList = WallpaperStore.pickableImages()
        wallPickerOpen = true
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.height + 28
        clip: true

        Column {
            id: column
            x: 16
            y: 12
            width: parent.width - 32
            spacing: 8

            Text { text: "连接"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 16 }
            Rectangle {
                width: parent.width
                height: 112
                radius: 14
                color: SystemState.card
                border.width: 1 / Screen.devicePixelRatio
                border.color: SystemState.separator
                Column {
                    anchors.fill: parent
                    Rectangle {
                        width: parent.width
                        height: 56
                        color: "transparent"
                        Canvas {
                            id: settingsBtIcon
                            width: 16
                            height: 20
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            onPaint: {
                                const ctx = getContext("2d")
                                ctx.reset()
                                ctx.clearRect(0, 0, width, height)
                                ctx.strokeStyle = SystemState.bluetooth ? SystemState.tint : SystemState.secondary
                                ctx.lineWidth = 1.7
                                ctx.lineCap = "round"
                                ctx.lineJoin = "round"
                                const cx = 8
                                ctx.beginPath()
                                ctx.moveTo(cx, 1.4)
                                ctx.lineTo(cx, 18.6)
                                ctx.moveTo(cx, 5.6)
                                ctx.lineTo(12.6, 2.2)
                                ctx.moveTo(cx, 5.6)
                                ctx.lineTo(12.6, 9.1)
                                ctx.moveTo(cx, 14.2)
                                ctx.lineTo(12.6, 17.6)
                                ctx.moveTo(cx, 14.2)
                                ctx.lineTo(12.6, 10.6)
                                ctx.moveTo(cx, 5.6)
                                ctx.lineTo(3.4, 2.2)
                                ctx.moveTo(cx, 5.6)
                                ctx.lineTo(3.4, 9.1)
                                ctx.moveTo(cx, 14.2)
                                ctx.lineTo(3.4, 17.6)
                                ctx.moveTo(cx, 14.2)
                                ctx.lineTo(3.4, 10.6)
                                ctx.stroke()
                            }
                            Connections {
                                target: SystemState
                                function onBluetoothChanged() { settingsBtIcon.requestPaint() }
                                function onDarkChanged() { settingsBtIcon.requestPaint() }
                            }
                        }
                        Text {
                            anchors.left: settingsBtIcon.right
                            anchors.leftMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: "蓝牙"
                            color: SystemState.ink
                            font.pixelSize: 16
                        }
                        IosToggle {
                            anchors.right: parent.right
                            anchors.rightMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            checked: SystemState.bluetooth
                            onToggled: function (v) { SystemState.bluetooth = v }
                        }
                    }
                    Rectangle { x: 16; width: parent.width - 16; height: 1; color: SystemState.separator }
                    Rectangle {
                        width: parent.width
                        height: 55
                        color: "transparent"
                        Canvas {
                            id: settingsWifiIcon
                            width: 22
                            height: 16
                            anchors.left: parent.left
                            anchors.leftMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            onPaint: {
                                const ctx = getContext("2d")
                                ctx.reset()
                                ctx.clearRect(0, 0, width, height)
                                const cx = width * 0.5
                                const cy = height - 1.2
                                const up = SystemState.wifi && SystemState.wifiName.length > 0
                                const level = up ? SystemState.wifiSignal : 0
                                const active = SystemState.wifi ? SystemState.tint : SystemState.secondary
                                const idle = SystemState.secondary
                                const a0 = Math.PI * 1.22
                                const a1 = Math.PI * 1.78
                                ctx.lineCap = "round"
                                ctx.lineWidth = 2.0
                                const arcs = [4.2, 7.6, 11.0]
                                for (let i = 0; i < arcs.length; ++i) {
                                    const lit = up && level >= i + 1
                                    ctx.strokeStyle = lit ? active : idle
                                    ctx.globalAlpha = lit ? 1 : (SystemState.wifi ? 0.35 : 0.45)
                                    ctx.beginPath()
                                    ctx.arc(cx, cy, arcs[i], a0, a1)
                                    ctx.stroke()
                                }
                                ctx.globalAlpha = 1
                                ctx.fillStyle = (up && level >= 1) ? active : idle
                                ctx.globalAlpha = (up && level >= 1) ? 1 : 0.45
                                ctx.beginPath()
                                ctx.arc(cx, cy, 1.5, 0, Math.PI * 2)
                                ctx.fill()
                                ctx.globalAlpha = 1
                            }
                            Connections {
                                target: SystemState
                                function onWifiChanged() { settingsWifiIcon.requestPaint() }
                                function onWifiNameChanged() { settingsWifiIcon.requestPaint() }
                                function onWifiSignalChanged() { settingsWifiIcon.requestPaint() }
                                function onDarkChanged() { settingsWifiIcon.requestPaint() }
                            }
                        }
                        Text {
                            anchors.left: settingsWifiIcon.right
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: "无线局域网"
                            color: SystemState.ink
                            font.pixelSize: 16
                        }
                        Text {
                            anchors.right: wifiChevron.left
                            anchors.rightMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            text: !SystemState.wifi ? "关闭" : (SystemState.wifiName.length > 0 ? SystemState.wifiName : "未连接")
                            color: SystemState.secondary
                            font.pixelSize: 15
                        }
                        IosIcon {
                            id: wifiChevron
                            anchors.right: wifiSwitch.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            width: 12
                            height: 12
                            name: "chevron"
                            ink: SystemState.secondary
                        }
                        IosToggle {
                            id: wifiSwitch
                            anchors.right: parent.right
                            anchors.rightMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            checked: SystemState.wifi
                            onToggled: function (v) { SystemState.wifi = v }
                        }
                    }
                }
            }

            Item { width: 1; height: 6 }
            Text { text: "通知"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 16 }
            Rectangle {
                width: parent.width
                height: 56
                radius: 14
                color: SystemState.card
                border.width: 1 / Screen.devicePixelRatio
                border.color: SystemState.separator
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: "状态栏显示通知"
                    color: SystemState.ink
                    font.pixelSize: 16
                }
                Text {
                    anchors.right: notifySwitch.left
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: BluetoothMediaHub.phoneConnected ? "iPhone 通知" : "未连手机"
                    color: SystemState.secondary
                    font.pixelSize: 13
                }
                IosToggle {
                    id: notifySwitch
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    checked: NotificationSession.statusBarEnabled
                    onToggled: function (v) { NotificationSession.statusBarEnabled = v }
                }
            }

            Item { width: 1; height: 6 }
            Text { text: "外观"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 16 }
            Rectangle {
                width: parent.width
                height: SystemState.autoTheme ? 56 : 113
                radius: 14
                color: SystemState.card
                border.width: 1 / Screen.devicePixelRatio
                border.color: SystemState.separator
                clip: true
                Column {
                    width: parent.width
                    Rectangle {
                        width: parent.width
                        height: 56
                        color: "transparent"
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            text: "自动"
                            color: SystemState.ink
                            font.pixelSize: 16
                        }
                        Text {
                            anchors.right: autoSwitch.left
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: SystemState.dark ? "深色" : "浅色"
                            color: SystemState.secondary
                            font.pixelSize: 15
                        }
                        IosToggle {
                            id: autoSwitch
                            anchors.right: parent.right
                            anchors.rightMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            checked: SystemState.autoTheme
                            onToggled: function (v) { SystemState.autoTheme = v }
                        }
                    }
                    Rectangle {
                        x: 16
                        width: parent.width - 16
                        height: 1
                        color: SystemState.separator
                        visible: !SystemState.autoTheme
                    }
                    Rectangle {
                        width: parent.width
                        height: 56
                        color: "transparent"
                        visible: !SystemState.autoTheme
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            text: "手动"
                            color: SystemState.ink
                            font.pixelSize: 16
                        }
                        IosSegmented {
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            width: 168
                            labels: ["浅色", "深色"]
                            currentIndex: SystemState.manualDark ? 1 : 0
                            onActivated: function (index) { SystemState.manualDark = (index === 1) }
                        }
                    }
                }
            }

            Item { width: 1; height: 6 }
            Text { text: "GPS"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 16 }
            Rectangle {
                width: parent.width
                height: 140
                radius: 14
                color: SystemState.card
                border.width: 1 / Screen.devicePixelRatio
                border.color: SystemState.separator
                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10
                    Row {
                        width: parent.width
                        Text {
                            width: parent.width - 80
                            text: GpsSource.status
                            color: SystemState.ink
                            font.pixelSize: 15
                            elide: Text.ElideRight
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        IosToggle {
                            anchors.verticalCenter: parent.verticalCenter
                            checked: GpsSource.enabled
                            onToggled: function (v) { GpsSource.enabled = v }
                        }
                    }
                    Text {
                        text: GpsSource.hasFix
                              ? (GpsSource.latitude.toFixed(5) + ", " + GpsSource.longitude.toFixed(5)
                                 + " · " + Math.round(GpsSource.speedMps * 3.6) + " km/h")
                              : "无定位"
                        color: SystemState.secondary
                        font.pixelSize: 13
                    }
                    Row {
                        spacing: 10
                        IosSegmented {
                            width: 176
                            labels: ["演示", "串口"]
                            currentIndex: GpsSource.demoMode ? 0 : 1
                            onActivated: function (index) {
                                if (index === 0) {
                                    GpsSource.demoMode = true
                                } else {
                                    GpsSource.demoMode = false
                                    GpsSource.enabled = true
                                    GpsSource.refresh()
                                }
                            }
                        }
                        Text {
                            text: GpsSource.device
                            color: SystemState.secondary
                            font.pixelSize: 12
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            Item { width: 1; height: 6 }
            Text { text: "亮度"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 16 }
            Rectangle {
                width: parent.width
                height: 52
                radius: 14
                color: SystemState.card
                border.width: 1 / Screen.devicePixelRatio
                border.color: SystemState.separator
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: "亮度"
                    color: SystemState.ink
                    font.pixelSize: 16
                }
                Text {
                    anchors.right: brightnessSlider.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: Math.round(SystemState.brightness * 100) + "%"
                    color: SystemState.secondary
                    font.pixelSize: 14
                }
                Slider {
                    id: brightnessSlider
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 280
                    from: 0.15
                    to: 1
                    Component.onCompleted: value = SystemState.brightness
                    onMoved: SystemState.brightness = value
                    background: Rectangle {
                        x: brightnessSlider.leftPadding
                        y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                        width: brightnessSlider.availableWidth
                        height: 4
                        radius: 2
                        color: SystemState.fill
                        Rectangle {
                            width: brightnessSlider.visualPosition * parent.width
                            height: parent.height
                            radius: 2
                            color: SystemState.tint
                        }
                    }
                    handle: Rectangle {
                        x: brightnessSlider.leftPadding + brightnessSlider.visualPosition * (brightnessSlider.availableWidth - width)
                        y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                        width: 22
                        height: 22
                        radius: 11
                        color: "#FFFFFF"
                        border.color: SystemState.separator
                        border.width: 0.5
                    }
                }
            }

            Item { width: 1; height: 6 }
            Text { text: "声音"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 16 }
            Rectangle {
                width: parent.width
                height: 52
                radius: 14
                color: SystemState.card
                border.width: 1 / Screen.devicePixelRatio
                border.color: SystemState.separator
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: "音量"
                    color: SystemState.ink
                    font.pixelSize: 16
                }
                Slider {
                    id: volumeSlider
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 280
                    from: 0
                    to: 1
                    Component.onCompleted: value = SystemState.volume
                    onMoved: SystemState.volume = value
                    background: Rectangle {
                        x: volumeSlider.leftPadding
                        y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                        width: volumeSlider.availableWidth
                        height: 4
                        radius: 2
                        color: SystemState.fill
                        Rectangle {
                            width: volumeSlider.visualPosition * parent.width
                            height: parent.height
                            radius: 2
                            color: SystemState.tint
                        }
                    }
                    handle: Rectangle {
                        x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
                        y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                        width: 22
                        height: 22
                        radius: 11
                        color: "#FFFFFF"
                        border.color: SystemState.separator
                        border.width: 0.5
                    }
                }
            }

            Item { width: 1; height: 6 }
            Text { text: "锁屏"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 16 }
            Rectangle {
                width: parent.width
                height: 96
                radius: 14
                color: SystemState.card
                border.width: 1 / Screen.devicePixelRatio
                border.color: SystemState.separator
                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10
                    Row {
                        width: parent.width
                        Text {
                            text: "超时时间"
                            color: SystemState.ink
                            font.pixelSize: 16
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Item { width: 12; height: 1 }
                        Text {
                            text: SystemState.lockTimeout <= 0 ? "永不" : (SystemState.lockTimeout + " 分钟")
                            color: SystemState.secondary
                            font.pixelSize: 14
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    IosSegmented {
                        width: parent.width
                        labels: ["1分", "2分", "5分", "10分", "30分", "永不"]
                        property var values: [1, 2, 5, 10, 30, 0]
                        currentIndex: {
                            const v = SystemState.lockTimeout
                            const i = values.indexOf(v)
                            return i >= 0 ? i : 2
                        }
                        onActivated: function (index) {
                            SystemState.lockTimeout = values[index]
                        }
                    }
                }
            }

            Item { width: 1; height: 6 }
            Text { text: "通用"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 16 }
            Rectangle {
                width: parent.width
                height: aboutCol.implicitHeight + 24
                radius: 14
                color: SystemState.card
                border.width: 1 / Screen.devicePixelRatio
                border.color: SystemState.separator
                Column {
                    id: aboutCol
                    x: 16
                    y: 12
                    width: parent.width - 32
                    spacing: 6
                    Item {
                        width: parent.width
                        height: 22
                        IosPressable {
                            anchors.fill: parent
                            onClicked: SystemState.unlockDeveloper()
                            Item {
                                anchors.fill: parent
                                Text {
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "关于本机"
                                    color: SystemState.ink
                                    font.pixelSize: 16
                                }
                                IosIcon {
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 12
                                    height: 12
                                    name: "chevron"
                                    ink: SystemState.secondary
                                }
                            }
                        }
                    }
                    Text {
                        text: "MP157 IVI  " + SystemState.appVersion
                        color: SystemState.secondary
                        font.pixelSize: 14
                    }
                    Text {
                        text: "网络 " + (!SystemState.wifi ? "关闭" : (SystemState.wifiName.length ? SystemState.wifiName : "未连接"))
                              + " · 蓝牙 " + (SystemState.bluetooth ? "开" : "关")
                        color: SystemState.secondary
                        font.pixelSize: 13
                    }
                    Text {
                        visible: SystemState.developerMode
                        text: "开发者模式已开启"
                        color: SystemState.tint
                        font.pixelSize: 13
                    }
                }
            }

            Item { width: 1; height: 6; visible: SystemState.developerMode }
            Text {
                visible: SystemState.developerMode
                text: "天气调试"
                color: SystemState.secondary
                font.pixelSize: 13
                leftPadding: 16
            }
            Rectangle {
                visible: SystemState.developerMode
                width: parent.width
                height: weatherDebug.implicitHeight + 20
                radius: 14
                color: SystemState.card
                border.width: 1 / Screen.devicePixelRatio
                border.color: SystemState.separator
                Flow {
                    id: weatherDebug
                    x: 12
                    y: 10
                    width: parent.width - 24
                    spacing: 8
                    Repeater {
                        model: [
                            { mode: "", label: "实况" },
                            { mode: "clear", label: "晴" },
                            { mode: "night", label: "晴夜" },
                            { mode: "cloudy", label: "多云" },
                            { mode: "overcast", label: "阴" },
                            { mode: "fog", label: "雾" },
                            { mode: "haze", label: "霾" },
                            { mode: "dust", label: "沙尘" },
                            { mode: "sandLift", label: "扬沙" },
                            { mode: "wind", label: "大风" },
                            { mode: "blizzard", label: "暴雪" },
                            { mode: "frost", label: "霜" },
                            { mode: "ponding", label: "积水" },
                            { mode: "wetRoad", label: "湿滑" },
                            { mode: "typhoon", label: "台风" },
                            { mode: "rain", label: "小雨" },
                            { mode: "rainMid", label: "中雨" },
                            { mode: "rainHard", label: "暴雨" },
                            { mode: "thunder", label: "雷雨" },
                            { mode: "sleet", label: "雨夹雪" },
                            { mode: "freezeRain", label: "冻雨" },
                            { mode: "snow", label: "小雪" },
                            { mode: "snowMid", label: "中雪" },
                            { mode: "snowHard", label: "大雪" },
                            { mode: "hail", label: "冰雹" }
                        ]
                        delegate: IosPressable {
                            required property var modelData
                            width: 72
                            height: 32
                            onClicked: Weather.setPreview(modelData.mode)
                            Rectangle {
                                anchors.fill: parent
                                radius: 8
                                color: Weather.preview === modelData.mode ? SystemState.selected : SystemState.fill
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: Weather.preview === modelData.mode ? SystemState.tint : SystemState.ink
                                    font.pixelSize: 14
                                    font.bold: Weather.preview === modelData.mode
                                }
                            }
                        }
                    }
                }
            }

            Item { width: 1; height: 6; visible: SystemState.developerMode }
            Text {
                visible: SystemState.developerMode
                text: "更新清单 URL"
                color: SystemState.secondary
                font.pixelSize: 13
                leftPadding: 16
            }
            Rectangle {
                visible: SystemState.developerMode
                width: parent.width
                height: 60
                radius: 14
                color: SystemState.card
                border.width: 1 / Screen.devicePixelRatio
                border.color: SystemState.separator
                TextField {
                    id: manifestField
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 12
                    height: 36
                    placeholderText: "更新清单 URL"
                    color: SystemState.ink
                    placeholderTextColor: SystemState.secondary
                    text: UpdateService.manifestUrl
                    onEditingFinished: UpdateService.manifestUrl = text
                    background: Rectangle {
                        radius: 10
                        color: SystemState.fill
                    }
                }
            }

            Item { width: 1; height: 6 }
            Text { text: "软件更新"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 16 }
            Rectangle {
                width: parent.width
                height: updateCol.implicitHeight + 24
                radius: 14
                color: SystemState.card
                border.width: 1 / Screen.devicePixelRatio
                border.color: SystemState.separator
                Column {
                    id: updateCol
                    x: 16
                    y: 12
                    width: parent.width - 32
                    spacing: 10
                    Text {
                        text: UpdateService.statusText
                        color: SystemState.ink
                        font.pixelSize: 15
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }
                    Text {
                        visible: UpdateService.notes.length > 0 && (UpdateService.status === "available" || UpdateService.status === "ready")
                        text: UpdateService.notes
                        color: SystemState.secondary
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }
                    Rectangle {
                        visible: UpdateService.status === "downloading" || UpdateService.status === "verifying" || UpdateService.status === "applying"
                        width: parent.width
                        height: 6
                        radius: 3
                        color: SystemState.fill
                        Rectangle {
                            width: parent.width * UpdateService.progress
                            height: parent.height
                            radius: 3
                            color: SystemState.tint
                        }
                    }
                    Row {
                        spacing: 10
                        IosPressable {
                            width: 120
                            height: 34
                            enabled: !UpdateService.busy
                            onClicked: {
                                if (SystemState.developerMode)
                                    UpdateService.manifestUrl = manifestField.text
                                UpdateService.checkForUpdate()
                            }
                            Rectangle {
                                anchors.fill: parent
                                radius: 17
                                color: UpdateService.busy ? SystemState.fill : SystemState.tint
                                Text {
                                    anchors.centerIn: parent
                                    text: UpdateService.busy ? "检查中" : "检查更新"
                                    color: UpdateService.busy ? SystemState.secondary : "#FFFFFF"
                                    font.pixelSize: 14
                                }
                            }
                        }
                        IosPressable {
                            visible: UpdateService.status === "available"
                            width: 120
                            height: 34
                            enabled: !UpdateService.busy
                            onClicked: UpdateService.startUpdate()
                            Rectangle {
                                anchors.fill: parent
                                radius: 17
                                color: SystemState.success
                                Text {
                                    anchors.centerIn: parent
                                    text: "立即升级"
                                    color: "#FFFFFF"
                                    font.pixelSize: 14
                                }
                            }
                        }
                        IosPressable {
                            visible: UpdateService.busy
                            width: 80
                            height: 34
                            onClicked: UpdateService.cancel()
                            Rectangle {
                                anchors.fill: parent
                                radius: 17
                                color: SystemState.fill
                                Text {
                                    anchors.centerIn: parent
                                    text: "取消"
                                    color: SystemState.ink
                                    font.pixelSize: 14
                                }
                            }
                        }
                    }
                }
            }

            Item { width: 1; height: 6 }
            Text { text: "壁纸"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 16 }
            Rectangle {
                width: parent.width
                height: wallGrid.implicitHeight + 58
                radius: 14
                color: SystemState.card
                border.width: 1 / Screen.devicePixelRatio
                border.color: SystemState.separator
                Grid {
                    id: wallGrid
                    x: 12
                    y: 12
                    width: parent.width - 24
                    columns: 5
                    columnSpacing: 8
                    rowSpacing: 8
                    Repeater {
                        model: WallpaperStore.items
                        delegate: IosPressable {
                            required property var modelData
                            width: (wallGrid.width - 32) / 5
                            height: width * 0.62
                            onClicked: WallpaperStore.select(modelData.path)
                            Item {
                                anchors.fill: parent
                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 3
                                    source: modelData.path
                                    fillMode: Image.PreserveAspectCrop
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    color: "transparent"
                                    radius: 8
                                    border.width: 2
                                    border.color: WallpaperStore.current === modelData.path ? SystemState.tint : "transparent"
                                }
                                Text {
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 6
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: modelData.name
                                    color: "#FFFFFF"
                                    font.pixelSize: 11
                                    style: Text.Outline
                                    styleColor: "#99000000"
                                }
                                Rectangle {
                                    visible: modelData.custom === true
                                    anchors.top: parent.top
                                    anchors.right: parent.right
                                    anchors.margins: 2
                                    width: 22
                                    height: 22
                                    radius: 11
                                    color: "#CC000000"
                                    z: 2
                                    Text {
                                        anchors.centerIn: parent
                                        text: "×"
                                        color: "#FFFFFF"
                                        font.pixelSize: 14
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: WallpaperStore.removeCustom(modelData.path)
                                    }
                                }
                            }
                        }
                    }
                }
                IosPressable {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 10
                    width: 96
                    height: 30
                    onClicked: root.openWallPicker()
                    Rectangle {
                        anchors.fill: parent
                        radius: 15
                        color: SystemState.tint
                        Text { anchors.centerIn: parent; text: "上传壁纸"; color: "#FFFFFF"; font.pixelSize: 13 }
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: root.wallPickerOpen
        color: "#99000000"
        z: 30
        MouseArea {
            anchors.fill: parent
            onClicked: root.wallPickerOpen = false
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Math.min(parent.height * 0.72, 420)
            radius: 16
            color: SystemState.card
            border.width: 1 / Screen.devicePixelRatio
            border.color: SystemState.separator
            MouseArea { anchors.fill: parent }

            Column {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                Text {
                    text: "从文件选择壁纸"
                    color: SystemState.ink
                    font.pixelSize: 18
                    font.bold: true
                }
                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: "图片来自文件应用「图片 / 无线接收」"
                    color: SystemState.secondary
                    font.pixelSize: 13
                }

                Flickable {
                    width: parent.width
                    height: Math.max(80, parent.height - 140)
                    contentHeight: pickGrid.implicitHeight
                    clip: true
                    Grid {
                        id: pickGrid
                        width: parent.width
                        columns: 4
                        columnSpacing: 8
                        rowSpacing: 8
                        Repeater {
                            model: root.wallPickList
                            delegate: IosPressable {
                                required property var modelData
                                width: (pickGrid.width - 24) / 4
                                height: width * 0.7
                                onClicked: {
                                    WallpaperStore.importFrom(modelData.path)
                                    root.wallPickerOpen = false
                                }
                                Image {
                                    anchors.fill: parent
                                    source: modelData.url
                                    fillMode: Image.PreserveAspectCrop
                                }
                                Text {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 4
                                    elide: Text.ElideMiddle
                                    text: modelData.name
                                    color: "#FFFFFF"
                                    font.pixelSize: 11
                                    style: Text.Outline
                                    styleColor: "#99000000"
                                }
                            }
                        }
                    }
                }

                Text {
                    visible: root.wallPickList.length === 0
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: "暂无图片，请先在文件应用中添加"
                    color: SystemState.secondary
                    font.pixelSize: 13
                }

                IosPressable {
                    width: parent.width
                    height: 48
                    Rectangle {
                        anchors.fill: parent
                        radius: 12
                        color: SystemState.fill
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "取消"
                        color: SystemState.ink
                        font.pixelSize: 16
                    }
                    onClicked: root.wallPickerOpen = false
                }
            }
        }
    }

    Connections {
        target: SystemState
        function onBrightnessChanged() { brightnessSlider.value = SystemState.brightness }
        function onVolumeChanged() { volumeSlider.value = SystemState.volume }
    }
}

import QtQuick
import Ivi.Services 1.0

Item {
    id: root
    readonly property color ink: "#FFFFFF"
    readonly property color muted: "#CCFFFFFF"
    readonly property color glass: "#28FFFFFF"
    readonly property color glassStrong: "#3DFFFFFF"
    readonly property color chipOff: "#22FFFFFF"
    readonly property color chipOn: "#55FFFFFF"

    readonly property var palette: {
        const k = Weather.kind
        const day = Weather.day
        if (k === "thunder" || k === "typhoon")
            return { top: "#1B1B3A", mid: "#2A2A55", bottom: "#0E1028" }
        if (k === "rainHard" || k === "ponding")
            return { top: "#2F3E55", mid: "#3D516B", bottom: "#1A2433" }
        if (k === "rain" || k === "rainMid" || k === "sleet" || k === "freezeRain" || k === "wetRoad")
            return day
                ? { top: "#4A647F", mid: "#5B7894", bottom: "#33485E" }
                : { top: "#243447", mid: "#314559", bottom: "#15202C" }
        if (k === "snow" || k === "snowMid" || k === "snowHard" || k === "blizzard" || k === "hail" || k === "frost")
            return day
                ? { top: "#8FA5B8", mid: "#A9BBC9", bottom: "#6E879B" }
                : { top: "#3A4A5C", mid: "#4E6073", bottom: "#24303C" }
        if (k === "fog" || k === "haze" || k === "dust" || k === "sandLift")
            return day
                ? { top: "#8B8680", mid: "#A39E96", bottom: "#6B665F" }
                : { top: "#3A3835", mid: "#4E4B46", bottom: "#262421" }
        if (k === "overcast")
            return day
                ? { top: "#6B7C8C", mid: "#7E8F9E", bottom: "#4E5D6B" }
                : { top: "#2A3340", mid: "#3A4654", bottom: "#1A2129" }
        if (k === "cloudy" || k === "wind")
            return day
                ? { top: "#5BA3D9", mid: "#6BB0E0", bottom: "#3A7EB8" }
                : { top: "#1C2A44", mid: "#2A3B5C", bottom: "#121C30" }
        return day
            ? { top: "#47B5F5", mid: "#3AA0E8", bottom: "#1B78D0" }
            : { top: "#0B1428", mid: "#182848", bottom: "#060A16" }
    }

    function uvText(v) {
        if (v < 3)
            return "弱"
        if (v < 6)
            return "中等"
        if (v < 8)
            return "强"
        if (v < 11)
            return "很强"
        return "极强"
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: root.palette.top
                Behavior on color { ColorAnimation { duration: 700 } }
            }
            GradientStop {
                position: 0.45
                color: root.palette.mid
                Behavior on color { ColorAnimation { duration: 700 } }
            }
            GradientStop {
                position: 1.0
                color: root.palette.bottom
                Behavior on color { ColorAnimation { duration: 700 } }
            }
        }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: -80
        width: parent.width * 1.2
        height: parent.height * 0.55
        radius: width / 2
        opacity: Weather.day ? 0.18 : 0.08
        gradient: Gradient {
            GradientStop { position: 0.0; color: Weather.day ? "#FFFFFF" : "#6EA8FF" }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: -60
        anchors.top: parent.top
        anchors.topMargin: 40
        width: 220
        height: 220
        radius: 110
        visible: Weather.kind === "clear" && Weather.day
        opacity: 0.22
        color: "#FFE566"
    }

    Column {
        anchors.fill: parent
        anchors.margins: 12
        anchors.bottomMargin: 8
        spacing: 10

        Item {
            width: parent.width
            height: parent.height
            clip: true

            Flickable {
                id: flick
                anchors.fill: parent
                contentWidth: width
                contentHeight: Math.max(height + 1, mainCol.height + 28)
                clip: true
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.DragOverBounds
                rebound: Transition {
                    NumberAnimation {
                        properties: "x,y"
                        duration: 280
                        easing.type: Easing.OutCubic
                    }
                }

                onMovementEnded: {
                    if (contentY < -56 && !Weather.refreshing) {
                        Weather.refresh()
                        contentY = 0
                    }
                }

                Column {
                    id: mainCol
                    width: flick.width
                    y: 8
                    spacing: 12

                    Item {
                        width: parent.width
                        height: 28
                        opacity: (flick.contentY < -20 || Weather.refreshing) ? 1 : 0
                        Text {
                            anchors.centerIn: parent
                            text: Weather.refreshing ? "刷新中…" : (flick.contentY < -56 ? "松开刷新" : "下拉刷新")
                            color: root.muted
                            font.pixelSize: 13
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 8

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: Weather.place.length > 0 ? Weather.place : "定位中"
                            color: root.ink
                            font.pixelSize: 28
                            font.bold: true
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: Weather.statusText.length > 0
                            text: Weather.statusText
                            color: root.muted
                            font.pixelSize: 12
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 14
                            height: 64

                            WeatherIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 56
                                height: 56
                                kind: Weather.kind
                                day: Weather.day
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: Weather.condition.length > 0 ? Weather.condition : "加载中…"
                                color: root.ink
                                font.pixelSize: 24
                                font.bold: true
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: Weather.condition.length > 0 ? (Weather.temperature + "°") : "--"
                                color: root.ink
                                font.pixelSize: 48
                                font.bold: true
                            }
                        }
                    }

                    Flow {
                        width: parent.width - 24
                        x: 12
                        spacing: 8
                        Repeater {
                            model: [
                                { t: "体感", k: "feels" },
                                { t: "湿度", k: "humidity" },
                                { t: "紫外线", k: "uv" },
                                { t: "风", k: "wind" },
                                { t: "能见度", k: "vis" }
                            ]
                            delegate: Rectangle {
                                required property var modelData
                                width: Math.floor((mainCol.width - 24 - 32) / 5)
                                height: 52
                                radius: 12
                                color: root.glass
                                border.color: "#22FFFFFF"
                                border.width: 1
                                Column {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.t
                                        color: root.muted
                                        font.pixelSize: 12
                                    }
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.k === "feels" ? (Weather.feelsLike + "°")
                                              : modelData.k === "humidity" ? (Weather.humidity + "%")
                                              : modelData.k === "uv" ? (Weather.uvIndex.toFixed(1) + " " + root.uvText(Weather.uvIndex))
                                              : modelData.k === "wind" ? (Weather.windKmh + " km/h")
                                              : (Math.round(Weather.visibilityM / 1000) + " km")
                                        color: root.ink
                                        font.pixelSize: 14
                                        font.bold: true
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        x: 20
                        width: parent.width - 40
                        visible: Weather.travelAlert.length > 0
                        text: Weather.travelAlert
                        color: "#FFD60A"
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }

                    Rectangle {
                        x: 12
                        width: parent.width - 24
                        height: 132
                        radius: 16
                        color: root.glass
                        border.color: "#22FFFFFF"
                        border.width: 1
                        clip: true

                        Column {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            Text {
                                text: "24 小时预报"
                                color: root.muted
                                font.pixelSize: 13
                            }

                            Flickable {
                                width: parent.width
                                height: 90
                                contentWidth: hourRow.width
                                clip: true
                                flickableDirection: Flickable.HorizontalFlick
                                boundsBehavior: Flickable.StopAtBounds

                                Row {
                                    id: hourRow
                                    spacing: 6
                                    Repeater {
                                        model: Weather.hourlyForecast
                                        delegate: Item {
                                            required property var modelData
                                            width: 58
                                            height: 86
                                            Column {
                                                anchors.centerIn: parent
                                                spacing: 2
                                                Text {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: modelData.now ? "现在" : modelData.hour
                                                    color: root.muted
                                                    font.pixelSize: 12
                                                }
                                                WeatherIcon {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    width: 32
                                                    height: 32
                                                    kind: modelData.kind
                                                    day: modelData.day
                                                }
                                                Text {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: modelData.temp + "°"
                                                    color: root.ink
                                                    font.pixelSize: 16
                                                    font.bold: true
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        x: 12
                        width: parent.width - 24
                        radius: 16
                        color: root.glass
                        border.color: "#22FFFFFF"
                        border.width: 1
                        height: dailyCol.height + 24

                        Column {
                            id: dailyCol
                            x: 12
                            y: 12
                            width: parent.width - 24
                            spacing: 2

                            Text {
                                text: "7 日预报"
                                color: root.muted
                                font.pixelSize: 13
                            }

                            Repeater {
                                model: Weather.dailyForecast
                                delegate: Row {
                                    required property var modelData
                                    width: dailyCol.width
                                    height: 44
                                    spacing: 8

                                    Text {
                                        width: 44
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.weekday
                                        color: root.ink
                                        font.pixelSize: 15
                                        font.bold: true
                                    }
                                    Text {
                                        width: 40
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.date
                                        color: root.muted
                                        font.pixelSize: 13
                                    }
                                    WeatherIcon {
                                        width: 28
                                        height: 28
                                        anchors.verticalCenter: parent.verticalCenter
                                        kind: modelData.kind
                                        day: true
                                    }
                                    Text {
                                        width: Math.max(40, parent.width - 44 - 40 - 28 - 100 - 32)
                                        anchors.verticalCenter: parent.verticalCenter
                                        elide: Text.ElideRight
                                        text: modelData.condition
                                        color: root.muted
                                        font.pixelSize: 14
                                    }
                                    Text {
                                        width: 100
                                        anchors.verticalCenter: parent.verticalCenter
                                        horizontalAlignment: Text.AlignRight
                                        text: modelData.tempMax + "° / " + modelData.tempMin + "°"
                                        color: root.ink
                                        font.pixelSize: 15
                                        font.bold: true
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

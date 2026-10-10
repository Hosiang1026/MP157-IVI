import QtQuick
import Ivi.Services 1.0
import "pinyin.js" as Pinyin

Item {
    id: root
    property Item target: null
    property bool open: false
    property bool allowed: true
    readonly property real panelH: 330
    readonly property bool shown: open && !!target && allowed

    signal dismissed()

    property string mode: "cn"
    property bool shift: false
    property bool caps: false
    property string composing: ""
    property var candList: []
    property int candPage: 0
    readonly property int candPageSize: 6
    readonly property var pageCands: candList.slice(candPage * candPageSize, candPage * candPageSize + candPageSize)
    readonly property int candPages: Math.max(1, Math.ceil(candList.length / candPageSize))
    readonly property var rowLetters1: ["q","w","e","r","t","y","u","i","o","p"]
    readonly property var rowLetters2: ["a","s","d","f","g","h","j","k","l"]
    readonly property var rowLetters3: ["z","x","c","v","b","n","m"]
    readonly property var rowDigits: ["1","2","3","4","5","6","7","8","9","0"]
    readonly property var rowSym1: ["[","]","{","}","#","%","^","*","+","="]
    readonly property var rowSym2: ["_","\\","|","~","<",">","$","&","¥","€"]
    readonly property var rowSym3: [":",";","\"","'","?","!","(",")"]
    readonly property var t9Digits: ["1","2","3","4","5","6","7","8","9"]
    readonly property var t9Subs: ["，。","ABC","DEF","GHI","JKL","MNO","PQRS","TUV","WXYZ"]

    function resolveInput(item) {
        if (!item)
            return null
        if (typeof item.insert === "function" && item.cursorPosition !== undefined)
            return item
        if (item.contentItem)
            return resolveInput(item.contentItem)
        return null
    }

    function bindFrom(item) {
        const t = resolveInput(item)
        if (t) {
            target = t
            open = true
            return true
        }
        return false
    }

    function hide() {
        composing = ""
        candList = []
        candPage = 0
        open = false
        target = null
        dismissed()
    }

    function refreshCand() {
        candList = mode === "cn" ? Pinyin.candidates(composing) : []
        candPage = 0
    }

    function insertText(t) {
        if (!target || !t || t.length === 0)
            return
        const start = target.selectionStart !== undefined ? target.selectionStart : target.cursorPosition
        const end = target.selectionEnd !== undefined ? target.selectionEnd : target.cursorPosition
        if (end > start && typeof target.remove === "function")
            target.remove(start, end)
        target.insert(target.cursorPosition, t)
        if (typeof target.forceActiveFocus === "function")
            target.forceActiveFocus()
    }

    function backspace() {
        if (!target)
            return
        if (mode === "cn" && composing.length > 0) {
            composing = composing.substring(0, composing.length - 1)
            refreshCand()
            return
        }
        const start = target.selectionStart !== undefined ? target.selectionStart : target.cursorPosition
        const end = target.selectionEnd !== undefined ? target.selectionEnd : target.cursorPosition
        if (end > start)
            target.remove(start, end)
        else if (target.cursorPosition > 0)
            target.remove(target.cursorPosition - 1, target.cursorPosition)
        if (typeof target.forceActiveFocus === "function")
            target.forceActiveFocus()
    }

    function flushCompose(preferFirst) {
        if (mode !== "cn" || composing.length === 0)
            return false
        if (preferFirst && candList.length > 0)
            commitCand(candList[0])
        else {
            composing = ""
            candList = []
            candPage = 0
        }
        return true
    }

    function commitCand(ch) {
        insertText(ch)
        composing = ""
        candList = []
        candPage = 0
    }

    function typeT9(d) {
        if (d === "1") {
            flushCompose(true)
            insertText("，")
            return
        }
        if (d < "2" || d > "9")
            return
        composing += d
        refreshCand()
    }

    function typeKey(k) {
        if (!k || k.length === 0)
            return
        if (mode === "cn") {
            const lower = k.toLowerCase()
            if (lower.length === 1 && lower >= "a" && lower <= "z") {
                composing += lower
                refreshCand()
                return
            }
            flushCompose(true)
            insertText(k)
            return
        }
        if (mode === "en") {
            const up = caps || shift
            const out = up ? k.toUpperCase() : k.toLowerCase()
            if (shift && !caps)
                shift = false
            insertText(out)
            return
        }
        insertText(k)
    }

    function pressAction(act) {
        if (act === "back") {
            backspace()
            return
        }
        if (act === "space") {
            if (flushCompose(true))
                return
            insertText(" ")
            return
        }
        if (act === "enter") {
            flushCompose(true)
            const t = target
            hide()
            if (t)
                t.focus = false
            return
        }
        if (act === "shift") {
            if (shift)
                caps = !caps
            shift = !shift
            if (!shift)
                caps = false
            return
        }
        if (act === "mode_cn") {
            mode = "cn"
            shift = false
            caps = false
            return
        }
        if (act === "mode_en") {
            flushCompose(false)
            mode = "en"
            return
        }
        if (act === "mode_sym") {
            flushCompose(true)
            mode = mode === "sym" ? "cn" : "sym"
            return
        }
        if (act === "hide") {
            hide()
            return
        }
        if (act === "cand_prev") {
            if (candPage > 0)
                candPage--
            return
        }
        if (act === "cand_next") {
            if (candPage < candPages - 1)
                candPage++
            return
        }
    }

    function keyLabel(k) {
        if (mode === "en") {
            const up = caps || shift
            return up ? k.toUpperCase() : k.toLowerCase()
        }
        return k
    }

    height: shown ? panelH : 0
    visible: height > 1
    clip: true

    Behavior on height {
        NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
    }

    Rectangle {
        id: panel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: panelH
        color: SystemState.dark ? "#F01C1C1E" : "#F5F2F2F7"

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: SystemState.separator
        }

        Column {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            anchors.topMargin: 6
            anchors.bottomMargin: 8
            spacing: 6

            Item {
                width: parent.width
                height: 48

                Rectangle {
                    id: composeBox
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(52, composeLab.implicitWidth + 18)
                    height: 40
                    radius: 10
                    visible: root.mode === "cn" && root.composing.length > 0
                    color: SystemState.selected
                    Text {
                        id: composeLab
                        anchors.centerIn: parent
                        text: root.composing
                        color: SystemState.tint
                        font.pixelSize: 18
                        font.bold: true
                    }
                }

                ListView {
                    id: candView
                    anchors.left: composeBox.visible ? composeBox.right : parent.left
                    anchors.leftMargin: composeBox.visible ? 6 : 0
                    anchors.right: rightTools.left
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    height: 44
                    orientation: ListView.Horizontal
                    clip: true
                    spacing: 6
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.pageCands
                    visible: root.composing.length > 0

                    delegate: Rectangle {
                        width: Math.max(48, lab.implicitWidth + (index === 0 ? 22 : 18))
                        height: 42
                        radius: 10
                        color: index === 0 ? SystemState.tint : SystemState.fill

                        Text {
                            id: lab
                            anchors.centerIn: parent
                            text: (index + 1) + "." + modelData
                            color: index === 0 ? "#FFFFFF" : SystemState.ink
                            font.pixelSize: 18
                            font.bold: index === 0
                        }
                        MouseArea {
                            anchors.fill: parent
                            onPressed: root.commitCand(modelData)
                        }
                    }
                }

                Row {
                    id: rightTools
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 5

                    KeyBtn {
                        w: 42
                        h: 40
                        label: "‹"
                        muted: true
                        visible: root.composing.length > 0 && root.candPages > 1
                        onTap: root.pressAction("cand_prev")
                    }
                    KeyBtn {
                        w: 42
                        h: 40
                        label: "›"
                        muted: true
                        visible: root.composing.length > 0 && root.candPages > 1
                        onTap: root.pressAction("cand_next")
                    }
                    KeyBtn {
                        w: 48
                        h: 40
                        label: "⌄"
                        muted: true
                        onTap: root.pressAction("hide")
                    }
                }
            }

            Loader {
                width: parent.width
                height: 207
                sourceComponent: root.mode === "cn" ? t9Pad : (root.mode === "sym" ? symPad : letterPad)
            }

            Row {
                width: parent.width
                spacing: 5
                KeyBtn {
                    w: 78
                    h: 48
                    label: root.mode === "cn" ? "中/英" : (root.mode === "en" ? "英/中" : "返回")
                    muted: true
                    active: root.mode === "cn"
                    onTap: {
                        if (root.mode === "sym" || root.mode === "en")
                            root.pressAction("mode_cn")
                        else
                            root.pressAction("mode_en")
                    }
                }
                KeyBtn {
                    w: 64
                    h: 48
                    label: root.mode === "sym" ? "拼音" : "符号"
                    muted: true
                    onTap: root.pressAction("mode_sym")
                }
                KeyBtn {
                    w: parent.width - 78 - 64 - 100 - 4 * 5
                    h: 48
                    label: root.mode === "cn" && root.composing.length > 0 ? "选定" : "空格"
                    onTap: root.pressAction("space")
                }
                KeyBtn {
                    w: 100
                    h: 48
                    label: "完成"
                    accent: true
                    onTap: root.pressAction("enter")
                }
            }
        }
    }

    Component {
        id: t9Pad
        Column {
            width: parent ? parent.width : 0
            spacing: 5
            Grid {
                width: parent.width
                columns: 3
                rowSpacing: 5
                columnSpacing: 5
                Repeater {
                    model: 9
                    T9Btn {
                        required property int index
                        w: (parent.width - 2 * 5) / 3
                        h: 48
                        digit: root.t9Digits[index]
                        sub: root.t9Subs[index]
                        onTap: root.typeT9(digit)
                    }
                }
            }
            Row {
                width: parent.width
                spacing: 5
                KeyBtn {
                    w: (parent.width - 2 * 5) / 3
                    h: 48
                    label: "0"
                    sub: "空格"
                    muted: true
                    onTap: root.pressAction("space")
                }
                KeyBtn {
                    w: (parent.width - 2 * 5) / 3
                    h: 48
                    label: "。"
                    muted: true
                    onTap: {
                        root.flushCompose(true)
                        root.insertText("。")
                    }
                }
                KeyBtn {
                    w: (parent.width - 2 * 5) / 3
                    h: 48
                    label: "⌫"
                    muted: true
                    repeat: true
                    onTap: root.pressAction("back")
                }
            }
        }
    }

    Component {
        id: letterPad
        Column {
            width: parent ? parent.width : 0
            spacing: 5
            Row {
                width: parent.width
                spacing: 5
                Repeater {
                    model: root.rowDigits
                    delegate: KeyBtn {
                        w: (parent.width - 9 * 5) / 10
                        h: 42
                        label: modelData
                        muted: true
                        onTap: root.insertText(modelData)
                    }
                }
            }
            Row {
                width: parent.width
                spacing: 5
                Repeater {
                    model: root.rowLetters1
                    delegate: KeyBtn {
                        w: (parent.width - 9 * 5) / 10
                        h: 48
                        label: root.keyLabel(modelData)
                        onTap: root.typeKey(modelData)
                    }
                }
            }
            Row {
                width: parent.width
                spacing: 5
                Item { width: 20; height: 1 }
                Repeater {
                    model: root.rowLetters2
                    delegate: KeyBtn {
                        w: (parent.width - 40 - 8 * 5) / 9
                        h: 48
                        label: root.keyLabel(modelData)
                        onTap: root.typeKey(modelData)
                    }
                }
            }
            Row {
                width: parent.width
                spacing: 5
                KeyBtn {
                    w: 68
                    h: 48
                    label: root.caps ? "大写" : "⇧"
                    muted: true
                    active: root.shift || root.caps
                    onTap: root.pressAction("shift")
                }
                Repeater {
                    model: root.rowLetters3
                    delegate: KeyBtn {
                        w: (parent.width - 68 - 68 - 8 * 5) / 7
                        h: 48
                        label: root.keyLabel(modelData)
                        onTap: root.typeKey(modelData)
                    }
                }
                KeyBtn {
                    w: 68
                    h: 48
                    label: "⌫"
                    muted: true
                    repeat: true
                    onTap: root.pressAction("back")
                }
            }
        }
    }

    Component {
        id: symPad
        Column {
            width: parent ? parent.width : 0
            spacing: 5
            Row {
                width: parent.width
                spacing: 5
                Repeater {
                    model: root.rowSym1
                    delegate: KeyBtn {
                        w: (parent.width - 9 * 5) / 10
                        h: 48
                        label: modelData
                        onTap: root.typeKey(modelData)
                    }
                }
            }
            Row {
                width: parent.width
                spacing: 5
                Repeater {
                    model: root.rowSym2
                    delegate: KeyBtn {
                        w: (parent.width - 9 * 5) / 10
                        h: 48
                        label: modelData
                        onTap: root.typeKey(modelData)
                    }
                }
            }
            Row {
                width: parent.width
                spacing: 5
                Repeater {
                    model: root.rowSym3
                    delegate: KeyBtn {
                        w: (parent.width - 68 - 7 * 5) / 8
                        h: 48
                        label: modelData
                        onTap: root.typeKey(modelData)
                    }
                }
                KeyBtn {
                    w: 68
                    h: 48
                    label: "⌫"
                    muted: true
                    repeat: true
                    onTap: root.pressAction("back")
                }
            }
            Item { width: 1; height: 48 }
        }
    }

    component T9Btn: Rectangle {
        id: t9
        property real w: 40
        property real h: 48
        property string digit: ""
        property string sub: ""
        signal tap()
        width: w
        height: h
        radius: 10
        color: tapArea.pressed ? (SystemState.dark ? "#66666A" : "#C7C7CC")
                               : (SystemState.dark ? "#2C2C2E" : "#FFFFFF")
        border.width: SystemState.dark ? 0 : 0.6
        border.color: SystemState.separator
        Column {
            anchors.centerIn: parent
            spacing: 1
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: t9.digit
                color: SystemState.ink
                font.pixelSize: 22
                font.bold: true
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: t9.sub
                color: SystemState.secondary
                font.pixelSize: 11
                visible: t9.sub.length > 0
            }
        }
        MouseArea {
            id: tapArea
            anchors.fill: parent
            onPressed: t9.tap()
        }
    }

    component KeyBtn: Rectangle {
        id: kb
        property real w: 40
        property real h: 44
        property string label: ""
        property string sub: ""
        property bool muted: false
        property bool accent: false
        property bool active: false
        property bool repeat: false
        signal tap()
        width: w
        height: h
        radius: 10
        color: {
            if (tapArea.pressed)
                return SystemState.dark ? "#66666A" : "#C7C7CC"
            if (accent)
                return SystemState.tint
            if (active)
                return SystemState.selected
            if (muted)
                return SystemState.dark ? "#3A3A3C" : "#D8D8DC"
            return SystemState.dark ? "#2C2C2E" : "#FFFFFF"
        }
        border.width: SystemState.dark ? 0 : 0.6
        border.color: SystemState.separator
        Column {
            anchors.centerIn: parent
            spacing: 1
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: kb.label
                color: kb.accent ? "#FFFFFF" : SystemState.ink
                font.pixelSize: kb.sub.length > 0 ? 20 : (kb.label.length > 2 ? 15 : (kb.label.length > 1 ? 17 : 22))
                font.bold: kb.label.length <= 2
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: kb.sub
                color: SystemState.secondary
                font.pixelSize: 11
                visible: kb.sub.length > 0
            }
        }
        Timer {
            id: holdDelay
            interval: 380
            onTriggered: holdRepeat.start()
        }
        Timer {
            id: holdRepeat
            interval: 65
            repeat: true
            onTriggered: kb.tap()
        }
        MouseArea {
            id: tapArea
            anchors.fill: parent
            onPressed: {
                kb.tap()
                if (kb.repeat)
                    holdDelay.start()
            }
            onReleased: {
                holdDelay.stop()
                holdRepeat.stop()
            }
            onCanceled: {
                holdDelay.stop()
                holdRepeat.stop()
            }
        }
    }
}

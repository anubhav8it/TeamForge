pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Shapes
import TeamForge

// Workspace choice: the first thing TeamForge shows. Local only; nothing to sign in to.
// A live but quiet backdrop (drifting colour fields, a dot grid with parallax), the 3D logo,
// the wordmark and three workspace cards that tilt towards the pointer.
Rectangle {
    id: entry

    signal chosen(string workspace)

    color: Theme.background

    readonly property var choices: [
        { key: "participant", title: qsTr("PARTICIPANT"), text: qsTr("Build your profile and discover opportunities"), glyph: "◉" },
        { key: "host", title: qsTr("HOST"), text: qsTr("Create projects and build teams"), glyph: "✦" },
        { key: "developer", title: qsTr("DEVELOPER"), text: qsTr("Monitor, test and manage TeamForge"), glyph: "⌘" }
    ]
    readonly property bool narrow: width < 980
    readonly property bool shortWindow: height < 760
    // Pointer position over the screen, each in [-1, 1] (0 when the pointer is away).
    property real pointerX: 0
    property real pointerY: 0
    property string choosing: ""
    property real intro: 0 // entrance progress, 0..1

    function choose(key) {
        // The browser build limits workspaces to the signed-in account's role (checked in C++).
        if (choosing !== "" || !Backend.workspaceAllowed(key))
            return
        choosing = key
        chooseTimer.restart()
    }

    Timer {
        id: chooseTimer
        interval: 340
        onTriggered: entry.chosen(entry.choosing)
    }

    // Replay the entrance each time the screen is shown again.
    onVisibleChanged: {
        if (visible) {
            choosing = ""
            introAnimation.restart()
        }
    }
    Component.onCompleted: introAnimation.restart()
    NumberAnimation {
        id: introAnimation
        target: entry
        property: "intro"
        from: 0
        to: 1
        duration: 1100
        easing.type: Easing.OutCubic
    }
    // Entrance timing for item `n` of the sequence: 0 before its turn, 1 once it has arrived.
    function stage(n: int): real {
        return Math.max(0, Math.min(1, entry.intro * 1.9 - n * 0.16))
    }

    // Every pointer move steers the logo (relative to the logo itself, so it turns towards the
    // pointer wherever it is) and shifts the backdrop slightly for depth.
    HoverHandler {
        id: screenHover
        onPointChanged: {
            entry.pointerX = (point.position.x / entry.width) * 2 - 1
            entry.pointerY = (point.position.y / entry.height) * 2 - 1
            const local = logo.mapFromItem(entry, point.position.x, point.position.y)
            logo.lookAt((local.x - logo.width / 2) / (logo.width * 0.9), (local.y - logo.height / 2) / (logo.height * 1.4))
        }
        onHoveredChanged: {
            if (!hovered) {
                entry.pointerX = 0
                entry.pointerY = 0
                logo.rest()
            }
        }
    }
    Behavior on pointerX { SmoothedAnimation { velocity: 3 } }
    Behavior on pointerY { SmoothedAnimation { velocity: 3 } }

    // --- backdrop --------------------------------------------------------------------------------

    component Glow: Shape {
        id: blob
        property color tone
        property real strength: 0.16
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1
            fillGradient: RadialGradient {
                centerX: blob.width / 2
                centerY: blob.height / 2
                centerRadius: blob.width / 2
                focalX: centerX
                focalY: centerY
                GradientStop { position: 0.0; color: Qt.rgba(blob.tone.r, blob.tone.g, blob.tone.b, blob.strength) }
                GradientStop { position: 0.55; color: Qt.rgba(blob.tone.r, blob.tone.g, blob.tone.b, blob.strength * 0.3) }
                GradientStop { position: 1.0; color: "transparent" }
            }
            PathRectangle { width: blob.width; height: blob.height }
        }
    }

    Item {
        id: backdrop
        anchors.fill: parent
        property real drift: 0
        NumberAnimation on drift {
            from: 0
            to: Math.PI * 2
            duration: 26000
            loops: Animation.Infinite
        }

        Glow {
            tone: Theme.accent
            strength: 0.20
            width: entry.width * 0.9
            height: width
            x: -width * 0.38 + Math.cos(backdrop.drift) * 40 - entry.pointerX * 18
            y: -height * 0.5 + Math.sin(backdrop.drift) * 30 - entry.pointerY * 12
        }
        Glow {
            tone: Theme.violet
            strength: 0.16
            width: entry.width * 0.8
            height: width
            x: entry.width - width * 0.6 + Math.sin(backdrop.drift) * 50 - entry.pointerX * 26
            y: entry.height - height * 0.45 + Math.cos(backdrop.drift) * 30 - entry.pointerY * 18
        }
        Glow {
            tone: Theme.info
            strength: 0.08
            width: entry.width * 0.5
            height: width
            x: entry.width * 0.62 + Math.cos(backdrop.drift + 1.5) * 60
            y: -height * 0.3 + Math.sin(backdrop.drift + 1.5) * 40
        }

        // Dot grid, brightest near the centre; slight parallax against the pointer.
        Canvas {
            id: grid
            width: entry.width + 40
            height: entry.height + 40
            x: -20 - entry.pointerX * 10
            y: -20 - entry.pointerY * 10
            renderStrategy: Canvas.Cooperative
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                const step = 30
                const cx = width / 2
                const cy = height * 0.42
                const reach = Math.max(width, height) * 0.62
                for (let x = step / 2; x < width; x += step) {
                    for (let y = step / 2; y < height; y += step) {
                        const d = Math.sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) / reach
                        const alpha = Math.max(0, 0.10 * (1 - d))
                        if (alpha < 0.01)
                            continue
                        ctx.fillStyle = Qt.rgba(1, 1, 1, alpha)
                        ctx.fillRect(x, y, 1.6, 1.6)
                    }
                }
            }
        }
    }

    // --- content ---------------------------------------------------------------------------------

    ColumnLayout {
        id: content
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -8
        width: Math.min(entry.width - 64, 1180)
        spacing: 0

        LogoMark3D {
            id: logo
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.round(Math.max(170, Math.min(300, entry.height * (entry.shortWindow ? 0.25 : 0.29))))
            Layout.preferredHeight: Math.round(Layout.preferredWidth * 454 / 844)
            opacity: entry.stage(0)
            scale: 0.86 + 0.14 * entry.stage(0)

        }

        TfText {
            text: "TEAMFORGE"
            font.pixelSize: entry.shortWindow ? 56 : 66
            font.weight: Font.ExtraBold
            font.letterSpacing: 10 + (1 - entry.stage(1)) * 10
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: entry.shortWindow ? 26 : 38
            opacity: entry.stage(1)
        }
        TfText {
            text: qsTr("Build the right team.")
            font.pixelSize: entry.shortWindow ? 25 : 29
            font.weight: Font.Bold
            color: Theme.textSecondary
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 2
            opacity: entry.stage(2)
            transform: Translate { y: (1 - entry.stage(2)) * 14 }
        }

        TfText {
            text: qsTr("Choose your workspace")
            font.pixelSize: Theme.fontMd
            font.weight: Font.DemiBold
            color: Theme.textMuted
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: entry.shortWindow ? 30 : 44
            Layout.bottomMargin: 16
            opacity: entry.stage(3)
        }

        GridLayout {
            columns: entry.narrow ? 1 : 3
            columnSpacing: 20
            rowSpacing: 14
            Layout.fillWidth: true

            Repeater {
                model: entry.choices
                delegate: Item {
                    id: card
                    required property var modelData
                    required property int index
                    readonly property color tone: Theme.workspaceColor(modelData.key)
                    readonly property bool lastUsed: Backend.lastWorkspace === modelData.key
                    readonly property bool allowed: Backend.workspaceAllowed(modelData.key)
                    readonly property bool hot: hover.hovered || activeFocus
                    readonly property bool picked: entry.choosing === modelData.key
                    readonly property bool dimmed: entry.choosing !== "" && !picked
                    property real tiltX: 0
                    property real tiltY: 0
                    Behavior on tiltX { SpringAnimation { spring: 3; damping: 0.3; epsilon: 0.02 } }
                    Behavior on tiltY { SpringAnimation { spring: 3; damping: 0.3; epsilon: 0.02 } }

                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.title
                    Accessible.description: allowed ? modelData.text : qsTr("Not available for this account")
                    Accessible.onPressAction: entry.choose(card.modelData.key)
                    activeFocusOnTab: true
                    Keys.onReturnPressed: entry.choose(modelData.key)
                    Keys.onEnterPressed: entry.choose(modelData.key)
                    Keys.onSpacePressed: entry.choose(modelData.key)

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    // At least the designed height; taller when the text wraps.
                    implicitHeight: Math.max(entry.narrow ? 120 : entry.shortWindow ? 214 : 244,
                                             cardLayout.implicitHeight + (entry.narrow ? 40 : 52))

                    readonly property real arrive: entry.stage(4 + index * 0.6)
                    opacity: arrive * (dimmed || !allowed ? 0.35 : 1)
                    Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
                    scale: picked ? 1.035 : dimmed ? 0.97 : hot ? 1.015 : 1
                    Behavior on scale { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }

                    transform: [
                        Translate {
                            y: (1 - card.arrive) * 30 + (card.hot && !entry.narrow ? -6 : 0)
                            Behavior on y { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
                        },
                        Rotation {
                            origin.x: card.width / 2
                            origin.y: card.height / 2
                            axis { x: 1; y: 0; z: 0 }
                            angle: card.tiltX
                        },
                        Rotation {
                            origin.x: card.width / 2
                            origin.y: card.height / 2
                            axis { x: 0; y: 1; z: 0 }
                            angle: card.tiltY
                        }
                    ]

                    // Elevation: a soft shadow that deepens on hover.
                    Rectangle {
                        anchors.fill: surface
                        anchors.topMargin: card.hot ? 14 : 6
                        radius: surface.radius
                        color: "#000000"
                        opacity: card.hot ? 0.45 : 0.22
                        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
                        Behavior on anchors.topMargin { NumberAnimation { duration: Theme.animNormal } }
                        layer.enabled: true
                        layer.effect: MultiEffect { blurEnabled: true; blur: 1.0; blurMax: 40 }
                    }

                    Rectangle {
                        id: surface
                        anchors.fill: parent
                        radius: 20
                        color: card.picked ? Qt.tint(Theme.surfaceRaised, Qt.rgba(card.tone.r, card.tone.g, card.tone.b, 0.22))
                             : card.hot ? Theme.surfaceRaised : Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.92)
                        border.width: card.hot || card.picked ? 2 : 1
                        border.color: card.hot || card.picked ? card.tone : Theme.border
                        Behavior on color { ColorAnimation { duration: Theme.animNormal } }
                        Behavior on border.color { ColorAnimation { duration: Theme.animNormal } }
                        clip: true

                        // Accent line that grows across the top on hover.
                        Rectangle {
                            height: 3
                            width: card.hot || card.picked ? parent.width : 64
                            x: card.hot || card.picked ? 0 : 24
                            radius: 1.5
                            color: card.tone
                            opacity: card.hot || card.picked ? 1 : 0.7
                            Behavior on width { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.OutCubic } }
                            Behavior on x { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.OutCubic } }
                        }
                        // Light that follows the pointer inside the card.
                        Rectangle {
                            visible: card.hot
                            width: 320
                            height: 320
                            radius: 160
                            x: hover.point.position.x - width / 2
                            y: hover.point.position.y - height / 2
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: Qt.rgba(card.tone.r, card.tone.g, card.tone.b, 0.10) }
                                GradientStop { position: 1.0; color: "transparent" }
                            }
                        }

                        GridLayout {
                            id: cardLayout
                            anchors.fill: parent
                            anchors.margins: entry.narrow ? 20 : 26
                            columns: entry.narrow ? 3 : 1
                            columnSpacing: 18
                            rowSpacing: 12

                            RowLayout {
                                spacing: 12
                                Rectangle {
                                    implicitWidth: 56
                                    implicitHeight: 56
                                    radius: 18
                                    color: Qt.rgba(card.tone.r, card.tone.g, card.tone.b, card.hot ? 0.26 : 0.16)
                                    Behavior on color { ColorAnimation { duration: Theme.animNormal } }
                                    Text {
                                        anchors.centerIn: parent
                                        text: card.modelData.glyph
                                        font.pixelSize: 26
                                        color: Qt.lighter(card.tone, 1.25)
                                        rotation: card.hot ? -8 : 0
                                        Behavior on rotation { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutBack } }
                                    }
                                }
                                Item { Layout.fillWidth: true; visible: !entry.narrow }
                                Chip {
                                    visible: card.lastUsed && !entry.narrow
                                    text: qsTr("Last used")
                                    tone: "tint"
                                    tint: card.tone
                                }
                            }
                            ColumnLayout {
                                spacing: 6
                                Layout.fillWidth: true
                                TfText {
                                    text: card.modelData.title
                                    font.pixelSize: entry.shortWindow ? 24 : 27
                                    font.weight: Font.ExtraBold
                                    font.letterSpacing: 1.6
                                }
                                TfText {
                                    text: card.modelData.text
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontMd
                                    font.weight: Font.Medium
                                    wrapMode: Text.WordWrap
                                    elide: Text.ElideNone
                                    Layout.fillWidth: true
                                }
                            }
                            Item { Layout.fillHeight: true; visible: !entry.narrow }
                            RowLayout {
                                spacing: 8
                                Layout.alignment: entry.narrow ? Qt.AlignVCenter : Qt.AlignLeft
                                TfText {
                                    text: card.picked ? qsTr("Opening…") : qsTr("Enter workspace")
                                    font.pixelSize: Theme.fontBase
                                    font.weight: Font.ExtraBold
                                    color: card.hot || card.picked ? Qt.lighter(card.tone, 1.25) : Theme.text
                                    Behavior on color { ColorAnimation { duration: Theme.animFast } }
                                }
                                TfText {
                                    text: "→"
                                    font.pixelSize: Theme.fontLg
                                    font.weight: Font.ExtraBold
                                    color: card.hot || card.picked ? Qt.lighter(card.tone, 1.25) : Theme.textSecondary
                                    transform: Translate {
                                        x: card.hot || card.picked ? 6 : 0
                                        Behavior on x { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutBack } }
                                    }
                                }
                            }
                        }
                    }

                    // Focus ring for keyboard users.
                    Rectangle {
                        anchors.fill: surface
                        anchors.margins: -4
                        radius: surface.radius + 4
                        color: "transparent"
                        border.width: 2
                        border.color: card.tone
                        visible: card.activeFocus
                        opacity: 0.6
                    }

                    HoverHandler {
                        id: hover
                        cursorShape: Qt.PointingHandCursor
                        onPointChanged: {
                            if (!hovered || entry.narrow)
                                return
                            card.tiltY = (point.position.x / card.width - 0.5) * 7
                            card.tiltX = -(point.position.y / card.height - 0.5) * 7
                        }
                        onHoveredChanged: if (!hovered) { card.tiltX = 0; card.tiltY = 0 }
                    }
                    TapHandler {
                        id: tap
                        onTapped: entry.choose(card.modelData.key)
                    }
                }
            }
        }

        TfText {
            text: qsTr("Runs locally on this device  ·  %1 participants  ·  %2 projects  ·  %3 skills")
                  .arg(Backend.students.length).arg(Backend.requirements.length).arg(Backend.skillCount)
            font.pixelSize: Theme.fontSm
            font.weight: Font.DemiBold
            color: Theme.textMuted
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: entry.shortWindow ? 24 : 34
            opacity: entry.stage(6)
        }
    }
}

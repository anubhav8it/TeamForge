pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import TeamForge

// A ten-word motivational quote with depth, for the forms (onboarding, skills, joining, project
// creation). The backend picks the quote (core/Motivation.h); this only draws it.
//
// Each word is extruded (stacked copies, lit from the top left) and flips up into place; the
// quote's key word carries the category's gradient and a soft glow. Behind it: two drifting colour
// fields, a dot texture, rising particles and a glass sheen, and the card tilts towards the pointer.
// Plain Qt Quick, no Qt Quick 3D. Reduced motion keeps the look but stops every animation;
// very narrow cards and touch devices (Backend.lightVisuals) drop the costlier layers.
//
// It never takes focus or blocks input: the form around it works exactly as without it.
Item {
    id: card

    // "register" | "skills" | "join" | "host_project"
    property string moment: "skills"
    property var skills: []      // [{skill, level}] as entered; only counts and levels are read
    property int learningCount: 0
    property bool compact: width < 560

    // A fresh token each time the card is shown: the quote stays put while the form is open and
    // changes on the next visit.
    property int visit: -1
    readonly property var quote: visit >= 0 && visible ? Backend.motivation(moment, skills, learningCount, visit) : ({})
    // On screen: swapped in after the old quote's exit animation.
    property var shown: ({})
    property var shownWords: []

    readonly property var style: Theme.quoteStyle(shown.category ?? "")
    readonly property bool animated: !Backend.reduceMotion
    // Costlier layers (gradient key word, glow, dot texture, shadows) need room and a desktop GPU.
    readonly property bool effects: !Backend.lightVisuals && width >= 360
    readonly property bool moving: animated && effects && visible
    readonly property int depth: effects ? 4 : 2
    readonly property int fontSize: compact ? 19 : 23
    readonly property real cornerRadius: Theme.radius + 4

    property real tiltX: 0
    property real tiltY: 0

    implicitWidth: 640
    implicitHeight: content.implicitHeight + (compact ? 28 : 34)

    Accessible.role: Accessible.StaticText
    Accessible.name: shown.text ?? ""

    function renew() {
        visit = Backend.newMotivationVisit()
    }
    function swap() {
        shown = quote
        shownWords = quote.words ?? []
    }

    onVisibleChanged: {
        if (visible) {
            renew()
        } else {
            visit = -1
            shown = {}
            shownWords = []
        }
    }
    Component.onCompleted: if (visible) renew()

    onQuoteChanged: {
        const text = quote.text ?? ""
        if (text === "" || text === (shown.text ?? ""))
            return
        if (!animated || (shown.text ?? "") === "")
            swap()
        else
            exit.restart()
    }

    // The old quote lifts and fades, then the new one flips in word by word.
    SequentialAnimation {
        id: exit
        ParallelAnimation {
            NumberAnimation { target: words; property: "opacity"; to: 0; duration: 170; easing.type: Easing.InCubic }
            NumberAnimation { target: wordsShift; property: "y"; to: -10; duration: 170; easing.type: Easing.InCubic }
        }
        ScriptAction { script: card.swap() }
        PropertyAction { target: words; property: "opacity"; value: 1 }
        PropertyAction { target: wordsShift; property: "y"; value: 0 }
    }

    Behavior on tiltX {
        enabled: card.animated
        SpringAnimation { spring: 3; damping: 0.35; epsilon: 0.02 }
    }
    Behavior on tiltY {
        enabled: card.animated
        SpringAnimation { spring: 3; damping: 0.35; epsilon: 0.02 }
    }

    HoverHandler {
        id: hover
        enabled: card.moving
        onPointChanged: {
            card.tiltY = (point.position.x / Math.max(1, card.width) * 2 - 1) * 4
            card.tiltX = -(point.position.y / Math.max(1, card.height) * 2 - 1) * 5
        }
        onHoveredChanged: if (!hovered) { card.tiltX = 0; card.tiltY = 0 }
    }

    component Glow: Shape {
        id: blob
        property color tone
        property real strength: 0.3
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
                GradientStop { position: 0.5; color: Qt.rgba(blob.tone.r, blob.tone.g, blob.tone.b, blob.strength * 0.35) }
                GradientStop { position: 1.0; color: "transparent" }
            }
            PathRectangle { width: blob.width; height: blob.height }
        }
    }

    Item {
        id: stage
        anchors.fill: parent
        transform: [
            Rotation {
                origin.x: stage.width / 2
                origin.y: stage.height / 2
                axis { x: 1; y: 0; z: 0 }
                angle: card.tiltX
            },
            Rotation {
                origin.x: stage.width / 2
                origin.y: stage.height / 2
                axis { x: 0; y: 1; z: 0 }
                angle: card.tiltY
            }
        ]

        // Lift: a soft shadow under the card, deeper while the pointer is over it.
        Rectangle {
            visible: card.effects
            x: 14
            width: parent.width - 28
            y: parent.height - height * 0.5 + (hover.hovered ? 6 : 2)
            height: 26
            radius: 13
            color: "#000000"
            opacity: hover.hovered ? 0.42 : 0.3
            layer.enabled: card.effects
            layer.effect: MultiEffect { blurEnabled: true; blur: 1.0; blurMax: 32 }
            Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
        }

        // Rounded mask for the backdrop layers below.
        Rectangle {
            id: roundMask
            anchors.fill: parent
            radius: card.cornerRadius
            visible: false
            layer.enabled: true
        }

        // Backdrop: graphite glass with the category's colour fields drifting behind it.
        Item {
            id: surface
            anchors.fill: parent
            layer.enabled: true
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: roundMask
                maskThresholdMin: 0.5
                maskSpreadAtMin: 1.0
            }

            property real drift // animated below
            NumberAnimation on drift {
                running: card.moving
                from: 0
                to: Math.PI * 2
                duration: 18000
                loops: Animation.Infinite
            }

            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.surfaceRaised }
                    GradientStop { position: 1.0; color: Theme.surface }
                }
            }
            Glow {
                tone: card.style.from
                strength: 0.34
                width: Math.max(surface.width * 0.7, 320)
                height: width
                x: -width * 0.32 + Math.cos(surface.drift) * 36 - card.tiltY * 6
                y: -height * 0.55 + Math.sin(surface.drift) * 22 + card.tiltX * 6
            }
            Glow {
                tone: card.style.to
                strength: 0.28
                width: Math.max(surface.width * 0.6, 280)
                height: width
                x: surface.width - width * 0.62 + Math.sin(surface.drift) * 44 - card.tiltY * 10
                y: surface.height - height * 0.42 + Math.cos(surface.drift) * 18 + card.tiltX * 10
            }

            // Fine dot texture, painted once per size.
            Canvas {
                anchors.fill: parent
                visible: card.effects
                renderStrategy: Canvas.Cooperative
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    ctx.fillStyle = "rgba(255, 255, 255, 0.055)"
                    for (let y = 9; y < height; y += 16) {
                        for (let x = 9 + ((y / 16) % 2) * 8; x < width; x += 16) {
                            ctx.beginPath()
                            ctx.arc(x, y, 1, 0, Math.PI * 2)
                            ctx.fill()
                        }
                    }
                }
            }

            // Particles rising through the glass.
            Repeater {
                model: card.moving ? 11 : 0
                delegate: Rectangle {
                    id: particle
                    required property int index
                    readonly property real spread: (index * 0.618034) % 1
                    property real t // 0..1 along the rise, animated below
                    width: 3 + (index % 3) * 1.5
                    height: width
                    radius: width / 2
                    color: index % 2 === 0 ? card.style.from : card.style.to
                    x: surface.width * (0.04 + spread * 0.92)
                    y: surface.height * (1.08 - t * 1.2)
                    opacity: Math.sin(t * Math.PI) * 0.6
                    SequentialAnimation on t {
                        loops: Animation.Infinite
                        PauseAnimation { duration: particle.index * 520 }
                        NumberAnimation { from: 0; to: 1; duration: 6200 + particle.index * 680; easing.type: Easing.Linear }
                    }
                }
            }

            // Glass sheen across the top.
            Rectangle {
                width: parent.width
                height: parent.height * 0.55
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#14FFFFFF" }
                    GradientStop { position: 1.0; color: "#00FFFFFF" }
                }
            }

            // A large quotation mark, set back from the text.
            Text {
                text: "“"
                textFormat: Text.PlainText
                font.family: Theme.fontFamily
                font.pixelSize: card.compact ? 96 : 150
                font.weight: Font.ExtraBold
                color: card.style.to
                opacity: 0.13
                x: parent.width - width - (card.compact ? 10 : 22) - card.tiltY * 3
                y: (card.compact ? -26 : -44) + card.tiltX * 3
                Accessible.ignored: true
            }
        }

        // Edge: the category colour along the border, a lighter line where the light hits.
        Rectangle {
            anchors.fill: parent
            radius: card.cornerRadius
            color: "transparent"
            border.width: 1
            border.color: Qt.rgba(card.style.from.r, card.style.from.g, card.style.from.b, hover.hovered ? 0.6 : 0.38)
            Behavior on border.color { ColorAnimation { duration: Theme.animNormal } }
        }
        Rectangle {
            x: card.cornerRadius
            width: parent.width - card.cornerRadius * 2
            height: 1
            y: 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#00FFFFFF" }
                GradientStop { position: 0.3; color: "#33FFFFFF" }
                GradientStop { position: 1.0; color: "#00FFFFFF" }
            }
        }

        Column {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: card.compact ? 18 : 24
            anchors.rightMargin: card.compact ? 18 : 24
            spacing: card.compact ? 6 : 8

            // Category label with a pulsing light.
            Row {
                spacing: 8
                Rectangle {
                    id: lamp
                    anchors.verticalCenter: parent.verticalCenter
                    width: 8
                    height: 8
                    radius: 4
                    color: card.style.from
                    SequentialAnimation on opacity {
                        running: card.moving
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.35; duration: 1100; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: 1100; easing.type: Easing.InOutSine }
                    }
                    layer.enabled: card.effects
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: card.style.from
                        shadowBlur: 0.8
                        shadowHorizontalOffset: 0
                        shadowVerticalOffset: 0
                        blurMax: 12
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: card.style.label
                    textFormat: Text.PlainText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontXs
                    font.weight: Font.ExtraBold
                    font.letterSpacing: 1.2
                    font.capitalization: Font.AllUppercase
                    color: Qt.lighter(card.style.from, 1.25)
                    Accessible.ignored: true
                }
            }

            Item {
                id: wordsShift
                width: parent.width
                height: words.implicitHeight

                Flow {
                    id: words
                    width: parent.width
                    spacing: card.compact ? 5 : 7
                    layer.enabled: card.effects
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: "#000000"
                        shadowOpacity: 0.5
                        shadowBlur: 0.55
                        shadowHorizontalOffset: 2
                        shadowVerticalOffset: 5
                        blurMax: 24
                    }

                    Repeater {
                        model: card.shownWords
                        delegate: Item {
                            id: word
                            required property string modelData
                            required property int index
                            readonly property bool key: index === (card.shown.highlightIndex ?? -1)
                            property real lift: wordHover.hovered ? 4 : 0
                            property real flip: card.animated ? 82 : 0
                            property real rise: card.animated ? 18 : 0

                            implicitWidth: face.implicitWidth + card.depth
                            implicitHeight: face.implicitHeight + card.depth
                            opacity: card.animated ? 0 : 1
                            transform: [
                                Rotation {
                                    origin.x: word.width / 2
                                    origin.y: word.height
                                    axis { x: 1; y: 0; z: 0 }
                                    angle: word.flip
                                },
                                Translate { y: word.rise - word.lift }
                            ]
                            Behavior on lift { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic } }

                            Component.onCompleted: if (card.animated) entrance.start()
                            SequentialAnimation {
                                id: entrance
                                PauseAnimation { duration: 60 + word.index * 55 }
                                ParallelAnimation {
                                    NumberAnimation { target: word; property: "opacity"; to: 1; duration: 320; easing.type: Easing.OutCubic }
                                    NumberAnimation { target: word; property: "flip"; to: 0; duration: 560; easing.type: Easing.OutBack }
                                    NumberAnimation { target: word; property: "rise"; to: 0; duration: 480; easing.type: Easing.OutCubic }
                                }
                            }

                            // Extrusion, back to front.
                            Repeater {
                                model: card.depth
                                delegate: Text {
                                    required property int index
                                    readonly property int layerDepth: card.depth - index
                                    x: layerDepth
                                    y: layerDepth
                                    text: word.modelData
                                    textFormat: Text.PlainText
                                    font: face.font
                                    color: word.key ? Qt.darker(card.style.to, 2.6 + layerDepth * 0.25)
                                                    : Qt.rgba(0.05, 0.06, 0.08, 0.95 - layerDepth * 0.12)
                                    Accessible.ignored: true
                                }
                            }

                            Text {
                                id: face
                                text: word.modelData
                                textFormat: Text.PlainText
                                font.family: Theme.fontFamily
                                font.pixelSize: card.fontSize
                                font.weight: Font.ExtraBold
                                font.letterSpacing: -0.4
                                color: word.key ? card.style.from
                                     : wordHover.hovered ? Qt.lighter(card.style.from, 1.5) : Theme.text
                                // The key word is drawn by the gradient below (when effects are on).
                                visible: !(word.key && card.effects)
                                layer.enabled: word.key && card.effects
                                Accessible.ignored: true
                                Behavior on color { ColorAnimation { duration: Theme.animFast } }
                            }

                            // Key word: glow, then the category gradient masked by the glyphs.
                            Loader {
                                anchors.fill: face
                                active: word.key && card.effects
                                sourceComponent: Item {
                                    MultiEffect {
                                        anchors.fill: parent
                                        source: face
                                        shadowEnabled: true
                                        shadowColor: card.style.to
                                        shadowOpacity: wordHover.hovered ? 1.0 : 0.75
                                        shadowBlur: 0.9
                                        shadowHorizontalOffset: 0
                                        shadowVerticalOffset: 0
                                        blurMax: 20
                                    }
                                    Rectangle {
                                        id: sweep
                                        anchors.fill: parent
                                        visible: false
                                        layer.enabled: true
                                        gradient: Gradient {
                                            orientation: Gradient.Horizontal
                                            GradientStop { position: 0.0; color: Qt.lighter(card.style.from, 1.2) }
                                            GradientStop { position: 1.0; color: Qt.lighter(card.style.to, 1.1) }
                                        }
                                    }
                                    // Its own copy of the glyphs: the glow above pads its source,
                                    // so sharing `face` would shrink the mask.
                                    Text {
                                        id: glyphs
                                        anchors.fill: parent
                                        text: word.modelData
                                        textFormat: Text.PlainText
                                        font: face.font
                                        visible: false
                                        layer.enabled: true
                                        Accessible.ignored: true
                                    }
                                    MultiEffect {
                                        anchors.fill: parent
                                        source: sweep
                                        maskEnabled: true
                                        maskSource: glyphs
                                        maskThresholdMin: 0.3
                                        maskSpreadAtMin: 0.4
                                    }
                                }
                            }

                            HoverHandler {
                                id: wordHover
                                enabled: card.moving
                            }
                        }
                    }
                }
            }
        }
    }
}

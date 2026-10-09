pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import TeamForge

// The TeamForge monogram with depth, for the entry screen. Three layers cut from the original
// artwork (tools/make_logo_layers.ps1): a dark silhouette stacked into a short extrusion, the
// light T/F and the red bar, which sits slightly in front. Every layer gets the same perspective
// tilt, offset by its depth, so tilting reads as a solid object with parallax.
//
// Point at it (lookAt) to tilt it, drag to turn it, click or tap for a 360° spin; it springs back
// to rest. Plain Qt Quick transforms and one masked sheen: no 3D model, no Qt Quick 3D.
Item {
    id: logo

    property real maxTilt: 22
    property real tiltX: 0 // degrees about the horizontal axis (pointer above / below)
    property real tiltY: 0 // degrees about the vertical axis (pointer left / right)
    property real spin: 0
    property real bob      // idle float, 0..1 (animated below)
    property int spinDirection: 1

    readonly property real turn: tiltY + spin
    readonly property real turnRad: turn * Math.PI / 180
    readonly property real tiltRad: tiltX * Math.PI / 180
    // Seen from behind during a spin: only the silhouette (the logo's back) shows.
    readonly property bool facingAway: Math.cos(turnRad) < 0
    readonly property real thickness: Math.max(8, width * 0.05)
    // Screen-space shift of the back face for the current tilt, plus a little at rest so the
    // logo always shows some depth (lit from the top left).
    readonly property real backX: -Math.sin(turnRad) * thickness + thickness * 0.3
    readonly property real backY: Math.sin(tiltRad) * thickness + thickness * 0.45

    implicitWidth: 300
    implicitHeight: Math.round(implicitWidth * 454 / 844)

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("TeamForge logo")
    Accessible.description: qsTr("Spins the logo")
    Accessible.onPressAction: logo.spinOnce(1)

    // Pointer position relative to the logo, each in [-1, 1].
    function lookAt(nx: real, ny: real) {
        if (drag.active)
            return
        tiltY = Math.max(-1, Math.min(1, nx)) * maxTilt
        tiltX = -Math.max(-1, Math.min(1, ny)) * maxTilt * 0.75
    }
    function rest() {
        if (!drag.active) {
            tiltX = 0
            tiltY = 0
        }
    }
    function spinOnce(direction: int) {
        if (spinAnimation.running)
            return
        spinDirection = direction >= 0 ? 1 : -1
        spinAnimation.restart()
    }

    // Springy but quick: the logo follows the pointer in real time and settles smoothly.
    Behavior on tiltX {
        enabled: !drag.active
        SpringAnimation { spring: 4.5; damping: 0.32; epsilon: 0.02 }
    }
    Behavior on tiltY {
        enabled: !drag.active
        SpringAnimation { spring: 4.5; damping: 0.32; epsilon: 0.02 }
    }


    SequentialAnimation on bob {
        loops: Animation.Infinite
        NumberAnimation { from: 0; to: 1; duration: 3200; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1; to: 0; duration: 3200; easing.type: Easing.InOutSine }
    }

    ParallelAnimation {
        id: spinAnimation
        NumberAnimation {
            target: logo
            property: "spin"
            from: 0
            to: 360 * logo.spinDirection
            duration: 1150
            easing.type: Easing.OutCubic
        }
        SequentialAnimation {
            NumberAnimation { target: pop; property: "xScale"; to: 1.06; duration: 220; easing.type: Easing.OutQuad }
            NumberAnimation { target: pop; property: "xScale"; to: 1; duration: 700; easing.type: Easing.OutBack }
        }
        onFinished: logo.spin = 0
    }

    // Soft red glow behind the mark; drifts against the tilt.
    Shape {
        id: glow
        width: logo.width * 1.7
        height: logo.height * 2.1
        x: (logo.width - width) / 2 - logo.tiltY * 1.4
        y: (logo.height - height) / 2 + logo.tiltX * 1.4
        opacity: 0.55 + logo.bob * 0.15
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1
            fillGradient: RadialGradient {
                centerX: glow.width / 2
                centerY: glow.height / 2
                centerRadius: Math.min(glow.width, glow.height) / 2
                focalX: centerX
                focalY: centerY
                GradientStop { position: 0.0; color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.30) }
                GradientStop { position: 0.45; color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08) }
                GradientStop { position: 1.0; color: "transparent" }
            }
            PathRectangle { width: glow.width; height: glow.height }
        }
    }

    // Contact shadow: tightens as the logo floats up.
    Rectangle {
        width: logo.width * (0.62 - logo.bob * 0.06)
        height: logo.height * 0.1
        radius: height / 2
        x: (logo.width - width) / 2
        y: logo.height + logo.height * 0.16
        color: "#000000"
        opacity: 0.30 - logo.bob * 0.08
        layer.enabled: true
        layer.effect: MultiEffect { blurEnabled: true; blur: 1.0; blurMax: 32 }
    }

    Item {
        id: stage
        width: logo.width
        height: logo.height
        y: -logo.bob * logo.height * 0.05
        // Lifts slightly while the pointer is over it.
        scale: hover.hovered ? 1.04 : 1
        Behavior on scale { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
        transform: Scale { id: pop; origin.x: stage.width / 2; origin.y: stage.height / 2; yScale: xScale }

        component Layer: Item {
            id: layerItem
            // Shift of this layer in screen space; the face is 0, the back is 1, in front < 0.
            property real depthFraction: 0
            width: stage.width
            height: stage.height
            x: logo.backX * depthFraction
            y: logo.backY * depthFraction
            transform: [
                Rotation {
                    origin.x: layerItem.width / 2
                    origin.y: layerItem.height / 2
                    axis { x: 1; y: 0; z: 0 }
                    angle: logo.tiltX
                },
                Rotation {
                    origin.x: layerItem.width / 2
                    origin.y: layerItem.height / 2
                    axis { x: 0; y: 1; z: 0 }
                    angle: logo.turn
                }
            ]
        }

        // Extrusion: the silhouette repeated from the back face forwards.
        Repeater {
            model: 7
            delegate: Layer {
                id: slice
                required property int index
                depthFraction: 1 - index / 7
                Image {
                    anchors.fill: parent
                    source: Theme.markDepth
                    sourceSize.width: 844
                    smooth: true
                    mipmap: true
                    opacity: 0.55 + 0.45 * (slice.index / 6)
                }
            }
        }

        // Face: the light T/F with a sheen that slides as the logo turns.
        Layer {
            visible: !logo.facingAway
            Image {
                id: face
                anchors.fill: parent
                source: Theme.markLight
                sourceSize.width: 844
                smooth: true
                mipmap: true
            }
            Item {
                id: sheen
                anchors.fill: parent
                visible: false
                layer.enabled: true
                Rectangle {
                    width: parent.width * 0.42
                    height: parent.height * 2.4
                    y: -parent.height * 0.7
                    x: parent.width * (0.32 + logo.tiltY / logo.maxTilt * 0.42) - width / 2
                    rotation: 24
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.55) }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }
            }
            MultiEffect {
                anchors.fill: parent
                source: sheen
                maskEnabled: true
                maskSource: face
                // Only where the logo is opaque (the source has a transparent background).
                maskThresholdMin: 0.4
                maskSpreadAtMin: 0.2
                opacity: 0.35 + Math.abs(logo.tiltY) / logo.maxTilt * 0.4
            }
        }

        // Red bar: raised slightly above the face, so it moves a little more (parallax).
        Layer {
            visible: !logo.facingAway
            depthFraction: -0.45
            Image {
                anchors.fill: parent
                source: Theme.markRed
                sourceSize.width: 844
                smooth: true
                mipmap: true
            }
        }
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }
    // Drag (mouse or touch) turns the logo directly; a quick flick spins it.
    DragHandler {
        id: drag
        target: null
        property real startX: 0
        property real startY: 0
        onActiveChanged: {
            if (active) {
                startX = logo.tiltX
                startY = logo.tiltY
            } else {
                const vx = centroid.velocity.x
                if (Math.abs(vx) > 900)
                    logo.spinOnce(vx > 0 ? 1 : -1)
                logo.rest()
            }
        }
        onTranslationChanged: {
            logo.tiltY = Math.max(-45, Math.min(45, startY + translation.x * 0.3))
            logo.tiltX = Math.max(-35, Math.min(35, startX - translation.y * 0.3))
        }
    }
    TapHandler {
        onTapped: logo.spinOnce(logo.tiltY < 0 ? -1 : 1)
    }
}

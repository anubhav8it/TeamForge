import QtQuick
import QtQuick.Shapes
import TeamForge

// A soft circle of coloured light (radial gradient to transparent) for backgrounds.
Shape {
    id: glow

    property color tone: Theme.accent
    property real strength: 0.16

    preferredRendererType: Shape.CurveRenderer
    ShapePath {
        strokeWidth: -1
        fillGradient: RadialGradient {
            centerX: glow.width / 2
            centerY: glow.height / 2
            centerRadius: glow.width / 2
            focalX: centerX
            focalY: centerY
            GradientStop { position: 0.0; color: Qt.rgba(glow.tone.r, glow.tone.g, glow.tone.b, glow.strength) }
            GradientStop { position: 0.55; color: Qt.rgba(glow.tone.r, glow.tone.g, glow.tone.b, glow.strength * 0.3) }
            GradientStop { position: 1.0; color: "transparent" }
        }
        PathRectangle { width: glow.width; height: glow.height }
    }
}

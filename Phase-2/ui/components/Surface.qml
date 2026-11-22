import QtQuick
import TeamForge

// The card surface every panel and card is built on: a body that is a little lighter at the top
// (as if lit from above), a hairline of light along the top edge, a border, and a layered shade
// underneath that deepens when `lifted` (hover). Cheap to draw: no blur, so it scales to long
// lists and to the browser build.
//   tint      optional accent wash over the body (a workspace or status colour)
//   elevated  draw the shade (off for flat, nested surfaces)
Rectangle {
    id: surface

    property bool elevated: true
    property bool lifted: false
    property color tint: "transparent"
    property color baseColor: Theme.surface

    radius: Theme.radius
    border.color: lifted ? Theme.borderStrong : Theme.border
    gradient: Gradient {
        GradientStop { position: 0.0; color: Qt.tint(Qt.lighter(surface.baseColor, 1.16), Qt.rgba(surface.tint.r, surface.tint.g, surface.tint.b, surface.tint.a * 0.14)) }
        GradientStop { position: 1.0; color: Qt.tint(surface.baseColor, Qt.rgba(surface.tint.r, surface.tint.g, surface.tint.b, surface.tint.a * 0.05)) }
    }
    Behavior on border.color { ColorAnimation { duration: Theme.animNormal } }

    // Shade: two soft layers below the card (negative z draws behind the parent).
    Rectangle {
        z: -1
        visible: surface.elevated
        anchors.fill: parent
        anchors.topMargin: surface.lifted ? 10 : 4
        anchors.bottomMargin: surface.lifted ? -14 : -6
        anchors.leftMargin: 2
        anchors.rightMargin: 2
        radius: surface.radius + 4
        color: Theme.shadow
        opacity: surface.lifted ? 0.55 : 0.32
        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
        Behavior on anchors.topMargin { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
        Behavior on anchors.bottomMargin { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
    }
    Rectangle {
        z: -1
        visible: surface.elevated
        anchors.fill: parent
        anchors.topMargin: 1
        anchors.bottomMargin: -2
        radius: surface.radius + 1
        color: Theme.shadow
        opacity: 0.45
    }
    // Light catching the top edge.
    Rectangle {
        x: surface.radius
        y: 1
        width: Math.max(0, surface.width - surface.radius * 2)
        height: 1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.5; color: surface.lifted ? "#1FFFFFFF" : Theme.highlight }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }
}

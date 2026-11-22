import QtQuick
import QtQuick.Controls.Basic
import TeamForge

Button {
    id: control

    // primary | secondary | ghost | danger
    property string variant: "secondary"
    property bool compact: false

    hoverEnabled: true
    implicitHeight: compact ? 36 : 42
    leftPadding: compact ? 14 : 18
    rightPadding: compact ? 14 : 18
    topPadding: 0
    bottomPadding: 0
    font.family: Theme.fontFamily
    font.pixelSize: compact ? Theme.fontSm : Theme.fontBase
    font.weight: Font.Bold

    // Press feedback: a quick, small dip.
    scale: down ? 0.97 : 1
    Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }

    contentItem: Text {
        text: control.text
        font: control.font
        textFormat: Text.PlainText
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        color: {
            if (!control.enabled)
                return Theme.textMuted
            switch (control.variant) {
            case "primary": return "#FFFFFF"
            case "danger": return control.hovered ? Theme.accentHover : Theme.accent
            case "ghost": return control.hovered ? Theme.text : Theme.textSecondary
            default: return Theme.text
            }
        }
    }

    // Primary: TeamForge red blending into coral, with a soft glow under it that grows on hover.
    // Secondary: a raised surface lit from the top. Ghost: only on hover. Danger: red outline.
    readonly property bool lit: variant === "primary" && enabled
    background: Item {
        implicitHeight: control.implicitHeight

        // glow under the primary button
        Rectangle {
            visible: control.lit
            anchors.fill: parent
            anchors.topMargin: control.hovered ? 6 : 4
            anchors.bottomMargin: control.hovered ? -8 : -4
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            radius: Theme.radiusSmall + 4
            color: Theme.accent
            opacity: control.down ? 0.12 : control.hovered ? 0.34 : 0.2
            Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
            Behavior on anchors.bottomMargin { NumberAnimation { duration: Theme.animNormal } }
        }

        Rectangle {
            id: face
            anchors.fill: parent
            radius: Theme.radiusSmall
            color: {
                switch (control.variant) {
                case "primary": return Theme.surfaceRaised // shown when disabled
                case "ghost":
                    return control.down ? Theme.surfaceRaised : control.hovered ? Theme.surfaceHover : "transparent"
                case "danger":
                    return control.hovered && control.enabled ? Theme.accentSubtle : "transparent"
                default:
                    return control.down ? Theme.surface : control.hovered && control.enabled ? Theme.surfaceHover : Theme.surfaceRaised
                }
            }
            gradient: control.lit ? primaryFill : control.variant === "secondary" && !control.down ? secondaryFill : null
            border.width: control.variant === "ghost" || control.lit ? 0 : 1
            border.color: control.variant === "danger" ? "#6A2A33" : control.hovered && control.enabled ? Theme.borderStrong : Theme.border
            Behavior on color { ColorAnimation { duration: Theme.animFast } }

            Gradient {
                id: primaryFill
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: control.down ? Theme.accentPressed : control.hovered ? Theme.accentHover : Theme.accent }
                GradientStop { position: 1.0; color: control.down ? Theme.accentPressed : control.hovered ? "#FF8A6B" : Theme.accentWarm }
            }
            Gradient {
                id: secondaryFill
                GradientStop { position: 0.0; color: Qt.lighter(control.hovered && control.enabled ? Theme.surfaceHover : Theme.surfaceRaised, 1.12) }
                GradientStop { position: 1.0; color: control.hovered && control.enabled ? Theme.surfaceHover : Theme.surfaceRaised }
            }
            // light along the top edge of filled buttons
            Rectangle {
                visible: control.lit || control.variant === "secondary"
                x: face.radius
                y: 1
                width: Math.max(0, face.width - face.radius * 2)
                height: 1
                color: control.lit ? "#40FFFFFF" : Theme.highlight
            }
        }

        // keyboard focus ring
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: Theme.radiusSmall + 3
            color: "transparent"
            border.width: 2
            border.color: Theme.focusRing
            visible: control.visualFocus
        }
    }
}

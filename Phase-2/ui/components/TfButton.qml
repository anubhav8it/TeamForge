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

    background: Rectangle {
        radius: Theme.radiusSmall
        color: {
            switch (control.variant) {
            case "primary":
                if (!control.enabled) return Theme.surfaceRaised
                return control.down ? Theme.accentPressed : control.hovered ? Theme.accentHover : Theme.accent
            case "ghost":
                return control.down ? Theme.surfaceRaised : control.hovered ? Theme.surfaceHover : "transparent"
            case "danger":
                return control.hovered && control.enabled ? Theme.accentSubtle : "transparent"
            default:
                return control.down ? Theme.surface : control.hovered && control.enabled ? Theme.surfaceHover : Theme.surfaceRaised
            }
        }
        border.width: control.variant === "ghost" || (control.variant === "primary" && control.enabled) ? 0 : 1
        border.color: control.variant === "danger" ? "#5A2A2E" : control.visualFocus ? Theme.accent : Theme.borderStrong
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
    }
}

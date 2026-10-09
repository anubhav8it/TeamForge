import QtQuick
import TeamForge

// Compact pill for skills, statuses and project types. `skill: true` shows the text in
// capitals (skills are always displayed that way; their stored names are lower case).
Rectangle {
    id: chip

    property string text
    property bool skill: false
    // neutral | success | warning | accent | info | muted | tint (uses `tint`)
    property string tone: "neutral"
    property color tint: Theme.textSecondary
    property bool large: false

    readonly property color toneColor: {
        switch (tone) {
        case "success": return Theme.success
        case "warning": return Theme.warning
        case "accent": return Theme.accentHover
        case "info": return Theme.info
        case "tint": return tint
        default: return Theme.textSecondary
        }
    }

    implicitWidth: label.implicitWidth + (large ? 24 : 20)
    implicitHeight: large ? 32 : 28
    radius: height / 2
    color: tone === "neutral" ? Theme.surfaceRaised
         : tone === "muted" ? "transparent"
         : Qt.rgba(toneColor.r, toneColor.g, toneColor.b, 0.16)
    border.width: tone === "muted" || tone === "neutral" ? 1 : 0
    border.color: tone === "neutral" ? Theme.border : Theme.borderStrong
    Behavior on color { ColorAnimation { duration: Theme.animFast } }

    Text {
        id: label
        anchors.centerIn: parent
        text: chip.skill ? Theme.skill(chip.text) : chip.text
        textFormat: Text.PlainText
        font.family: Theme.fontFamily
        font.pixelSize: chip.large ? Theme.fontSm : 13
        font.weight: Font.Bold
        font.letterSpacing: chip.skill ? 0.5 : 0
        color: chip.tone === "muted" ? Theme.textMuted
             : chip.tone === "neutral" ? Theme.text : chip.toneColor
    }
}

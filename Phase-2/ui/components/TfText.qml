import QtQuick
import TeamForge

// Plain-text label on the theme's type scale. Data from files is always shown as plain text.
Text {
    // title | section | heading | body | secondary | muted | label | eyebrow | number | score
    property string variant: "body"

    textFormat: Text.PlainText
    elide: Text.ElideRight
    verticalAlignment: Text.AlignVCenter
    font.family: Theme.fontFamily
    font.pixelSize: {
        switch (variant) {
        case "title": return Theme.fontXl
        case "section": return Theme.fontLg
        case "heading": return Theme.fontMd
        case "muted": return Theme.fontSm
        case "label":
        case "eyebrow": return Theme.fontXs
        case "number": return Theme.fontDisplay
        case "score": return Theme.fontScore
        default: return Theme.fontBase
        }
    }
    font.weight: {
        switch (variant) {
        case "title":
        case "number":
        case "score": return Font.ExtraBold
        case "section":
        case "heading":
        case "eyebrow": return Font.Bold
        case "label": return Font.DemiBold
        default: return Font.Medium
        }
    }
    font.letterSpacing: variant === "title" || variant === "number" ? -0.6
                      : variant === "score" || variant === "section" ? -0.3
                      : variant === "eyebrow" ? 0.8 : 0
    font.capitalization: variant === "eyebrow" ? Font.AllUppercase : Font.MixedCase
    color: {
        switch (variant) {
        case "secondary": return Theme.textSecondary
        case "muted":
        case "label": return Theme.textMuted
        case "eyebrow": return Theme.accentHover
        default: return Theme.text
        }
    }
}

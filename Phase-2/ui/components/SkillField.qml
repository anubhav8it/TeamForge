pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import TeamForge

// Text field with skill suggestions from the backend's skill tree (prefix search). Skills are
// displayed in capitals; the typed text itself is kept as entered (the core normalises it).
TfTextField {
    id: field

    readonly property var suggestions: activeFocus && text.trim().length > 0
                                       ? Backend.skillsWithPrefix(text).filter(s => s !== text.trim().toLowerCase())
                                       : []

    placeholderText: qsTr("Skill, e.g. Python")
    font.capitalization: text.length > 0 ? Font.AllUppercase : Font.MixedCase
    font.weight: Font.Bold
    font.letterSpacing: text.length > 0 ? 0.4 : 0

    Popup {
        id: popup
        y: field.height + 4
        width: Math.max(field.width, 200)
        padding: 4
        visible: field.suggestions.length > 0
        closePolicy: Popup.NoAutoClose
        implicitHeight: Math.min(list.contentHeight + 8, 220)
        background: Rectangle {
            radius: Theme.radiusSmall
            color: Theme.surfaceRaised
            border.color: Theme.borderStrong
        }
        contentItem: ListView {
            id: list
            clip: true
            model: field.suggestions
            delegate: ItemDelegate {
                id: suggestion
                required property string modelData
                width: ListView.view.width
                height: 32
                hoverEnabled: true
                focusPolicy: Qt.NoFocus // keep focus in the field so the list stays open
                contentItem: Text {
                    text: Theme.skill(suggestion.modelData)
                    textFormat: Text.PlainText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.Bold
                    font.letterSpacing: 0.4
                    color: suggestion.hovered ? Theme.text : Theme.textSecondary
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: Theme.radiusSmall - 2
                    color: suggestion.hovered ? Theme.surfaceHover : "transparent"
                }
                onClicked: {
                    field.text = suggestion.modelData
                    field.editingFinished()
                    field.focus = false
                }
            }
        }
    }
}

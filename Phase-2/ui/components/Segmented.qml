pragma ComponentBehavior: Bound

import QtQuick
import TeamForge

// Segmented switch with a sliding selection. `options` is a list of {text, value}.
Rectangle {
    id: segmented

    property var options: []
    property var currentValue
    signal activated(var value)

    readonly property int selectedIndex: options.findIndex(o => o.value === currentValue)

    implicitWidth: row.implicitWidth + 6
    implicitHeight: 34
    radius: Theme.radiusSmall
    color: Theme.input
    border.color: Theme.border

    // Selection pill: slides between options.
    Rectangle {
        // repeater.count makes this re-evaluate once the options exist.
        readonly property Item target: repeater.count > 0 && segmented.selectedIndex >= 0
                                       ? repeater.itemAt(segmented.selectedIndex) : null
        visible: target !== null
        x: row.x + (target?.x ?? 0)
        y: 3
        width: target?.width ?? 0
        height: segmented.height - 6
        radius: Theme.radiusSmall - 2
        color: Theme.surfaceHover
        border.color: Theme.borderStrong
        Behavior on x { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 0

        Repeater {
            id: repeater
            model: segmented.options
            delegate: Item {
                id: option
                required property var modelData
                required property int index
                readonly property bool selected: index === segmented.selectedIndex
                Accessible.role: Accessible.Button
                Accessible.name: modelData.text
                Accessible.onPressAction: if (!option.selected) segmented.activated(option.modelData.value)
                width: label.implicitWidth + 24
                height: segmented.height - 6

                Text {
                    id: label
                    anchors.centerIn: parent
                    text: option.modelData.text
                    textFormat: Text.PlainText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.Bold
                    color: option.selected ? Theme.text : hover.hovered ? Theme.textSecondary : Theme.textMuted
                    Behavior on color { ColorAnimation { duration: Theme.animFast } }
                }
                HoverHandler {
                    id: hover
                    cursorShape: Qt.PointingHandCursor
                }
                TapHandler {
                    onTapped: if (!option.selected) segmented.activated(option.modelData.value)
                }
            }
        }
    }
}

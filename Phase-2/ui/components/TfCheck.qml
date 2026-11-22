import QtQuick
import TeamForge

// A themed check box: a rounded square with a tick, and an optional label. Controlled: a click,
// tap or the accessibility press action emits toggled(newValue); the owner sets `checked`.
Row {
    id: check

    property bool checked: false
    property string text
    // When set (e.g. to the width a layout gives the check box), the label wraps to fit it.
    property real wrapWidth: 0
    signal toggled(bool checked)

    spacing: 10

    function toggle() {
        toggled(!checked)
    }

    Accessible.role: Accessible.CheckBox
    Accessible.name: text
    Accessible.checkable: true
    Accessible.checked: checked
    Accessible.onPressAction: toggle()

    Rectangle {
        id: box
        anchors.verticalCenter: parent.verticalCenter
        width: 22
        height: 22
        radius: 6
        color: check.checked ? Theme.info : hover.hovered ? Theme.surfaceHover : "transparent"
        border.width: check.checked ? 0 : 1.5
        border.color: hover.hovered ? Theme.textSecondary : Theme.borderStrong
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
        Text {
            anchors.centerIn: parent
            visible: check.checked
            text: "✓"
            font.family: Theme.fontFamily
            font.pixelSize: 14
            font.weight: Font.ExtraBold
            color: "#0B1A2A"
        }
    }
    TfText {
        id: label
        visible: check.text !== ""
        anchors.verticalCenter: parent.verticalCenter
        text: check.text
        font.weight: Font.Bold
        width: check.wrapWidth > 0 ? Math.min(implicitWidth, Math.max(40, check.wrapWidth - box.width - check.spacing)) : implicitWidth
        wrapMode: check.wrapWidth > 0 ? Text.WordWrap : Text.NoWrap
        elide: Text.ElideNone
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: check.toggle() }
}

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Sidebar entry showing its position in the product flow.
Rectangle {
    id: item

    property int step: 1
    // Shown in the badge; the step number unless a workspace gives its own symbol.
    property string glyph: String(step)
    property color selectedColor: Theme.accent
    property string text
    property string detail
    property bool selected: false
    property bool compact: false
    signal clicked()

    implicitHeight: compact ? 48 : 56
    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: item.clicked()
    radius: Theme.radiusSmall
    color: selected ? Theme.surfaceRaised : hover.hovered ? Theme.surface : "transparent"
    Behavior on color { ColorAnimation { duration: Theme.animFast } }

    opacity: enabled ? 1 : 0.4

    Rectangle {
        visible: item.selected
        anchors.verticalCenter: parent.verticalCenter
        x: -2
        width: 4
        height: parent.height - 20
        radius: 2
        color: item.selectedColor
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: item.compact ? 0 : 12
        anchors.rightMargin: item.compact ? 0 : 10
        spacing: 12

        Rectangle {
            implicitWidth: 28
            implicitHeight: 28
            radius: 14
            color: item.selected ? item.selectedColor : "transparent"
            border.width: item.selected ? 0 : 1.5
            border.color: hover.hovered ? Theme.textSecondary : Theme.borderStrong
            Layout.alignment: item.compact ? Qt.AlignHCenter : Qt.AlignVCenter
            Behavior on color { ColorAnimation { duration: Theme.animFast } }
            Text {
                anchors.centerIn: parent
                text: item.glyph
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSm
                font.weight: Font.ExtraBold
                color: item.selected ? "#FFFFFF" : Theme.textSecondary
            }
        }
        ColumnLayout {
            visible: !item.compact
            spacing: 0
            Layout.fillWidth: true
            TfText {
                text: item.text
                font.pixelSize: Theme.fontMd
                font.weight: Font.Bold
                color: item.selected || hover.hovered ? Theme.text : Theme.textSecondary
                Layout.fillWidth: true
            }
            TfText {
                visible: item.detail !== ""
                text: item.detail
                variant: "muted"
                font.pixelSize: Theme.fontXs
                Layout.fillWidth: true
            }
        }
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }
    ToolTip.visible: compact && hover.hovered
    ToolTip.text: text
    ToolTip.delay: 300
    TapHandler {
        enabled: item.enabled
        onTapped: item.clicked()
    }
}

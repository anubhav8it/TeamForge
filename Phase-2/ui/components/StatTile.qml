import QtQuick
import QtQuick.Layouts
import TeamForge

// Compact figure card: label, big value, optional detail line and accent bar.
Rectangle {
    id: tile

    property string label
    property string value
    property string detail
    property color accentColor: Theme.textSecondary

    implicitWidth: 180
    implicitHeight: column.implicitHeight + 32
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border

    Rectangle {
        x: 16
        y: 16
        width: 4
        height: column.implicitHeight
        radius: 2
        color: tile.accentColor
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 16
        anchors.leftMargin: 30
        spacing: 0
        TfText {
            text: tile.label
            variant: "label"
            Layout.fillWidth: true
        }
        TfText {
            text: tile.value
            font.pixelSize: 28
            font.weight: Font.ExtraBold
            font.letterSpacing: -0.6
            Layout.fillWidth: true
        }
        TfText {
            visible: tile.detail !== ""
            text: tile.detail
            variant: "muted"
            font.pixelSize: Theme.fontXs
            Layout.fillWidth: true
        }
    }
}

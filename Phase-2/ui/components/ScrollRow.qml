import QtQuick
import TeamForge

// A row of controls (tabs, filters) that scrolls sideways when it is wider than the screen,
// instead of overflowing it. Children are laid out left to right.
Flickable {
    id: scroller

    default property alias content: row.data
    property alias spacing: row.spacing

    implicitHeight: row.implicitHeight
    implicitWidth: row.implicitWidth
    contentWidth: row.implicitWidth
    contentHeight: row.implicitHeight
    flickableDirection: Flickable.HorizontalFlick
    boundsBehavior: Flickable.StopAtBounds
    interactive: contentWidth > width
    clip: true

    Row {
        id: row
        spacing: 12
    }
}

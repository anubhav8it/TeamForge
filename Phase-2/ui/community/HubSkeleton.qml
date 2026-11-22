import QtQuick
import QtQuick.Layouts
import TeamForge

// Loading placeholder: a few pulsing card shapes where the list will appear.
ColumnLayout {
    id: skeleton
    property int rows: 3
    property int rowHeight: 86
    spacing: 12

    Repeater {
        model: skeleton.rows
        delegate: Surface {
            id: row
            required property int index
            Layout.fillWidth: true
            implicitHeight: skeleton.rowHeight
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 18
                spacing: 14
                Rectangle { width: 40; height: 40; radius: 20; color: Theme.surfaceRaised }
                Column {
                    spacing: 10
                    anchors.verticalCenter: parent.verticalCenter
                    Rectangle { width: 220 - row.index * 30; height: 12; radius: 6; color: Theme.surfaceRaised }
                    Rectangle { width: 340 - row.index * 40; height: 10; radius: 5; color: Theme.surfaceRaised }
                }
            }
            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: skeleton.visible
                NumberAnimation { from: 1; to: 0.45; duration: 700; easing.type: Easing.InOutQuad }
                NumberAnimation { from: 0.45; to: 1; duration: 700; easing.type: Easing.InOutQuad }
            }
        }
    }
}

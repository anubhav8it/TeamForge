import QtQuick
import QtQuick.Layouts
import TeamForge

// Dismissible strip for Backend.lastError. Slides open when a message arrives.
Item {
    id: banner

    property string message
    signal dismissed()

    readonly property bool shown: message !== ""
    implicitHeight: shown ? strip.implicitHeight + 8 : 0
    clip: true
    Behavior on implicitHeight { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }

    Rectangle {
        id: strip
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        implicitHeight: row.implicitHeight + 16
        radius: Theme.radiusSmall
        color: "#33191C"
        border.color: "#6B2B30"
        visible: opacity > 0
        opacity: banner.shown ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

        RowLayout {
            id: row
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 6
            spacing: 12

            Rectangle {
                implicitWidth: 22
                implicitHeight: 22
                radius: 11
                color: Theme.accent
                Text {
                    anchors.centerIn: parent
                    text: "!"
                    font.family: Theme.fontFamily
                    font.pixelSize: 14
                    font.weight: Font.ExtraBold
                    color: "#FFFFFF"
                }
            }
            TfText {
                text: banner.message
                color: "#FFD9DB"
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                Layout.fillWidth: true
            }
            TfButton {
                text: qsTr("Dismiss")
                variant: "ghost"
                compact: true
                onClicked: banner.dismissed()
            }
        }
    }
}

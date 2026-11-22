import QtQuick
import TeamForge

// Small red count bubble (unread messages, pending decisions). Hidden at zero.
Rectangle {
    id: badge
    property int count: 0
    visible: count > 0
    implicitWidth: Math.max(20, label.implicitWidth + 10)
    implicitHeight: 20
    radius: 10
    color: Theme.accent
    Accessible.name: qsTr("%1 new").arg(count)
    TfText {
        id: label
        anchors.centerIn: parent
        text: badge.count > 99 ? "99+" : String(badge.count)
        color: "#FFFFFF"
        font.pixelSize: 12
        font.weight: Font.ExtraBold
    }
}

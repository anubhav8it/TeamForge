pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import TeamForge

// Bottom navigation on phones for the workspace's main sections (each with its symbol and a
// short name), with an unread count on Community. Large touch targets; the active section is
// lit in the workspace colour.
Rectangle {
    id: tabs

    property var pages: []
    property int currentIndex: 0
    property color tone: Theme.info
    signal navigate(int index)

    implicitHeight: 64
    color: "#F20A0F1A"
    Rectangle { width: parent.width; height: 1; color: Theme.border }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        spacing: 0
        Repeater {
            model: tabs.pages
            delegate: Item {
                id: tab
                required property var modelData
                required property int index
                readonly property bool selected: tabs.currentIndex === index
                Layout.fillWidth: true
                Layout.fillHeight: true
                Accessible.role: Accessible.PageTab
                Accessible.name: modelData.title
                Accessible.selected: selected
                Accessible.onPressAction: tabs.navigate(tab.index)

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 8
                    width: 54
                    height: 30
                    radius: 15
                    color: tab.selected ? Qt.rgba(tabs.tone.r, tabs.tone.g, tabs.tone.b, 0.18) : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.animFast } }
                    Text {
                        anchors.centerIn: parent
                        text: tab.modelData.glyph ?? String(tab.index + 1)
                        font.family: Theme.fontFamily
                        font.pixelSize: 17
                        font.weight: Font.ExtraBold
                        color: tab.selected ? Qt.lighter(tabs.tone, 1.2) : Theme.textMuted
                    }
                    HubBadge {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.rightMargin: -4
                        anchors.topMargin: -5
                        count: tab.modelData.key === "community" ? Hub.unreadMessages + Hub.pendingDecisions : 0
                    }
                }
                TfText {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 7
                    text: tab.modelData.short ?? tab.modelData.title
                    font.pixelSize: 12
                    font.weight: Font.Bold
                    color: tab.selected ? Theme.text : Theme.textMuted
                }
                TapHandler { onTapped: tabs.navigate(tab.index) }
            }
        }
    }
}

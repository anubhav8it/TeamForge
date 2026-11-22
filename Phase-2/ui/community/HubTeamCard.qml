pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import TeamForge

// A team in the browse grid: name, tagline, leader, open roles, size and whether it recruits.
Surface {
    id: card

    property var team: ({})
    signal opened()

    implicitHeight: column.implicitHeight + 36
    radius: Theme.radius
    lifted: hover.hovered
    border.color: hover.hovered ? Theme.borderStrong : Theme.border
    Behavior on color { ColorAnimation { duration: Theme.animFast } }
    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Team %1").arg(team.name ?? "")
    Accessible.onPressAction: card.opened()

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 18
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            HubAvatar { person: ({ name: card.team.name ?? "", color: Theme.avatarColor(card.team.name ?? "") }); size: 42 }
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true
                TfText { text: card.team.name ?? ""; variant: "heading"; Layout.fillWidth: true }
                TfText {
                    text: card.team.leader ? qsTr("Led by %1").arg(card.team.leader.name) : ""
                    variant: "muted"
                    font.pixelSize: Theme.fontXs
                    Layout.fillWidth: true
                }
            }
            HubStatus { visible: (card.team.myRole ?? "") !== ""; status: card.team.myRole ?? "" }
        }
        TfText {
            visible: (card.team.tagline ?? "") !== ""
            text: card.team.tagline ?? ""
            variant: "secondary"
            wrapMode: Text.WordWrap
            elide: Text.ElideRight
            maximumLineCount: 2
            Layout.fillWidth: true
        }
        Flow {
            Layout.fillWidth: true
            spacing: 6
            visible: (card.team.openRoles ?? []).length > 0
            Repeater {
                model: (card.team.openRoles ?? []).slice(0, 4)
                delegate: Chip { required property string modelData; text: modelData; tone: "info" }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            TfText {
                text: qsTr("%1 / %2 members").arg(card.team.memberCount ?? 0).arg(card.team.maxMembers ?? 0)
                variant: "muted"
                font.pixelSize: Theme.fontXs
            }
            TfText {
                visible: !!card.team.event
                text: card.team.event ? "· " + card.team.event.name : ""
                variant: "muted"
                font.pixelSize: Theme.fontXs
                Layout.fillWidth: true
            }
            Item { Layout.fillWidth: true; visible: !card.team.event }
            Chip {
                text: card.team.full ? qsTr("Full") : card.team.recruiting ? qsTr("Recruiting") : qsTr("Not recruiting")
                tone: card.team.full ? "muted" : card.team.recruiting ? "success" : "muted"
            }
        }
    }
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: card.opened() }
}

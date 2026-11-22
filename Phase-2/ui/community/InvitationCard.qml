pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import TeamForge

// One direct invitation. For the team's leaders (`leader: true`): who was invited, by whom, the
// status, and Withdraw while it is open. For the invited person: the team, the message, their own
// compatibility, and Accept / Decline.
Surface {
    id: card

    property var invitation: ({})
    property bool leader: true
    property var report: ({})          // the invited person's compatibility
    signal openProfile(var person)
    signal revoke(var invitation)
    signal accept(var invitation)
    signal decline(var invitation)
    signal openTeam(int teamId)

    readonly property var person: leader ? (invitation.person ?? {}) : ({})
    readonly property bool open: invitation.status === "pending"
    readonly property var team: invitation.team ?? ({})

    implicitHeight: column.implicitHeight + 32
    radius: Theme.radius
    color: Theme.surface
    border.color: open && !leader ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.45) : Theme.border

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Avatar {
                visible: card.leader
                person: card.person
                size: 44
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: card.openProfile(card.person) }
            }
            TeamAvatar { visible: !card.leader; name: card.invitation.targetName ?? ""; size: 44 }
            ColumnLayout {
                spacing: 1
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true
                    TfText {
                        text: card.leader ? (card.person.name ?? "") : (card.invitation.targetName ?? "")
                        variant: "heading"
                        Layout.fillWidth: true
                    }
                    Chip { visible: card.leader; text: qsTr("Invitation"); tone: "info" }
                    HubStatus { status: card.invitation.status ?? "" }
                }
                TfText {
                    text: (card.leader ? qsTr("Invited by %1").arg(card.invitation.invitedBy ? card.invitation.invitedBy.name : qsTr("a team admin"))
                                       : qsTr("From %1").arg(card.invitation.invitedBy ? card.invitation.invitedBy.name : qsTr("the team")))
                          + " · " + Hub.relativeTime(card.invitation.createdAt ?? 0)
                          + (card.open ? " · " + qsTr("open until %1").arg(Hub.dateText(card.invitation.expiresAt ?? 0)) : "")
                    variant: "muted"
                    font.pixelSize: Theme.fontXs
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        // the team, for the invited person
        TfText {
            visible: !card.leader && text !== ""
            text: card.team.tagline || card.team.description || ""
            variant: "secondary"
            wrapMode: Text.WordWrap
            elide: Text.ElideRight
            maximumLineCount: 3
            Layout.fillWidth: true
        }
        TfText {
            visible: !card.leader && card.team.name !== undefined
            text: (card.team.leader ? qsTr("Led by %1").arg(card.team.leader.name) + " · " : "")
                  + (card.team.full ? qsTr("Full right now") : qsTr("%n place(s) left", "", card.team.vacancies ?? 0))
            variant: "muted"
            font.pixelSize: Theme.fontXs
            Layout.fillWidth: true
        }
        Rectangle {
            visible: (card.invitation.message ?? "") !== "" || (card.invitation.response ?? "") !== ""
            Layout.fillWidth: true
            implicitHeight: messages.implicitHeight + 20
            radius: Theme.radiusSmall
            color: Theme.surfaceRaised
            ColumnLayout {
                id: messages
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6
                TfText { visible: (card.invitation.message ?? "") !== ""; text: qsTr("Message"); variant: "label" }
                TfText {
                    visible: (card.invitation.message ?? "") !== ""
                    text: card.invitation.message ?? ""
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                    Layout.fillWidth: true
                }
                TfText { visible: (card.invitation.response ?? "") !== ""; text: qsTr("Reply"); variant: "label" }
                TfText {
                    visible: (card.invitation.response ?? "") !== ""
                    text: card.invitation.response ?? ""
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                    Layout.fillWidth: true
                }
            }
        }
        CompatibilityView {
            visible: !card.leader && card.open && card.report.bandLabel !== undefined
            report: card.report
            forCandidate: true
            Layout.fillWidth: true
        }

        Flow {
            Layout.fillWidth: true
            spacing: 8
            TfButton {
                visible: card.leader
                text: qsTr("Profile")
                compact: true
                variant: "ghost"
                onClicked: card.openProfile(card.person)
            }
            TfButton {
                visible: card.leader && card.open
                text: qsTr("Withdraw invitation")
                compact: true
                variant: "danger"
                onClicked: card.revoke(card.invitation)
            }
            TfButton {
                visible: !card.leader
                text: qsTr("View team")
                compact: true
                variant: "ghost"
                onClicked: card.openTeam(card.invitation.teamId)
            }
            TfButton {
                visible: !card.leader && card.open
                text: qsTr("Accept and join")
                compact: true
                variant: "primary"
                enabled: !card.team.full
                onClicked: card.accept(card.invitation)
            }
            TfButton {
                visible: !card.leader && card.open
                text: qsTr("Decline")
                compact: true
                variant: "ghost"
                onClicked: card.decline(card.invitation)
            }
        }
    }
}

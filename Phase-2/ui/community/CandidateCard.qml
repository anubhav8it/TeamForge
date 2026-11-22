pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import TeamForge

// One registered person in Find members: their photo (the shared identity), name, introduction,
// top skills with levels, teams they are in, and their compatibility with the team (score and
// what they would cover). Tapping opens the full profile and report.
Surface {
    id: card

    property var person: ({})
    property var report: ({})
    property var participant: ({})    // Backend.participantCard(studentId): skills with levels, details
    property bool teamFull: false
    signal openProfile()
    signal invite()
    signal reviewApplication()

    readonly property bool known: report.enoughInformation === true
    readonly property var gapSkills: (report.matching ?? []).filter(f => f.fillsGap).map(f => Theme.skill(f.skill))
    readonly property var topSkills: (participant.skills ?? []).length > 0
        ? participant.skills.slice().sort((a, b) => b.level - a.level).slice(0, 4).map(s => Theme.skill(s.skill) + " · " + s.level)
        : (person.skills ?? []).slice(0, 4).map(s => Theme.skill(s))

    implicitHeight: column.implicitHeight + 32
    radius: Theme.radius
    lifted: hover.hovered
    border.color: hover.hovered ? Theme.borderStrong : Theme.border
    Behavior on border.color { ColorAnimation { duration: Theme.animFast } }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Avatar { person: card.person; size: 52 }
            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                TfText { text: card.person.name ?? ""; variant: "heading"; Layout.fillWidth: true }
                TfText {
                    text: [card.participant.role || card.person.headline, card.participant.program || card.person.program]
                          .filter(t => !!t).join(" · ")
                    variant: "muted"
                    font.pixelSize: Theme.fontXs
                    Layout.fillWidth: true
                }
            }
            ColumnLayout {
                spacing: 2
                Layout.alignment: Qt.AlignTop
                CoverageRing {
                    visible: card.known
                    size: 54
                    lineWidth: 5
                    value: card.report.score ?? 0
                    fillColor: Theme.scoreColor(card.report.score ?? 0)
                    Layout.alignment: Qt.AlignHCenter
                    Accessible.role: Accessible.Indicator
                    Accessible.name: qsTr("Compatibility %1 percent").arg(card.report.percent ?? 0)
                }
                TfText {
                    text: card.known ? (card.report.bandLabel ?? "") : qsTr("Not enough info")
                    font.pixelSize: 11
                    font.weight: Font.ExtraBold
                    color: card.known ? Theme.scoreColor(card.report.score ?? 0) : Theme.textMuted
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }

        TfText {
            visible: text !== ""
            text: card.person.bio || card.participant.summary || ""
            variant: "secondary"
            wrapMode: Text.WordWrap
            elide: Text.ElideRight
            maximumLineCount: 2
            Layout.fillWidth: true
        }
        Flow {
            visible: card.topSkills.length > 0
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: card.topSkills
                delegate: Chip { required property string modelData; text: modelData; skill: true }
            }
        }
        TfText {
            visible: card.gapSkills.length > 0
            text: qsTr("Would cover: %1").arg(card.gapSkills.join(", "))
            color: Theme.success
            font.weight: Font.Bold
            font.pixelSize: Theme.fontXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        TfText {
            visible: (card.person.teams ?? []).length > 0 || (card.person.links ?? []).length > 0
            text: [(card.person.teams ?? []).length > 0 ? qsTr("In %1").arg(card.person.teams.map(t => t.name).join(", ")) : "",
                   (card.person.links ?? []).length > 0 ? qsTr("%n portfolio link(s)", "", card.person.links.length) : ""]
                  .filter(t => t !== "").join(" · ")
            variant: "muted"
            font.pixelSize: Theme.fontXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            TfButton {
                text: qsTr("View profile")
                compact: true
                variant: "ghost"
                onClicked: card.openProfile()
            }
            TfButton {
                visible: !card.person.pendingApplication
                text: card.person.pendingInvitation ? qsTr("Invited") : qsTr("Invite")
                enabled: !card.person.pendingInvitation && !card.teamFull
                variant: "primary"
                compact: true
                onClicked: card.invite()
            }
            TfButton {
                visible: !!card.person.pendingApplication
                text: qsTr("Applied: review")
                compact: true
                onClicked: card.reviewApplication()
            }
        }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: card.openProfile() }
    Accessible.role: Accessible.Button
    Accessible.name: (person.name ?? "") + ", " + (known ? (report.bandLabel ?? "") : qsTr("not enough information"))
    Accessible.onPressAction: card.openProfile()
}

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// One project as an opportunity for the participant (a Backend.opportunities entry): name,
// type, tagline, team size, key skills and match %. The main action is expressing interest;
// once sent, the card shows where the request stands with the host.
Rectangle {
    id: card

    property var opportunity: ({})
    property bool compact: false
    signal viewOpportunity(string requirementId)
    signal whyThisMatch(string requirementId)
    signal expressInterest(string requirementId)
    readonly property string status: opportunity.interestStatus ?? ""

    readonly property color tone: Theme.typeColor(opportunity.type ?? "")
    readonly property real score: opportunity.score ?? 0

    implicitHeight: column.implicitHeight + 40
    radius: Theme.radius
    color: hover.hovered ? Theme.surfaceRaised : Theme.surface
    border.color: hover.hovered ? Theme.borderStrong : Theme.border
    Behavior on color { ColorAnimation { duration: Theme.animFast } }
    // Hover lifts the card a little.
    transform: Translate {
        y: hover.hovered ? -2 : 0
        Behavior on y { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 1
        height: 4
        radius: 2
        color: card.tone
        opacity: 0.85
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 20
        anchors.topMargin: 22
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true
                RowLayout {
                    spacing: 8
                    Rectangle {
                        implicitWidth: 8
                        implicitHeight: 8
                        radius: 4
                        color: card.tone
                    }
                    TfText {
                        text: card.opportunity.type || qsTr("Project")
                        font.pixelSize: Theme.fontXs
                        font.weight: Font.ExtraBold
                        color: card.tone
                    }
                    TfText {
                        text: "·  " + qsTr("Team of %1–%2").arg(card.opportunity.minTeamSize ?? 0).arg(card.opportunity.maxTeamSize ?? 0)
                        variant: "muted"
                        font.pixelSize: Theme.fontXs
                        font.weight: Font.Bold
                    }
                }
                TfText {
                    text: card.opportunity.name ?? ""
                    font.pixelSize: card.compact ? Theme.fontLg : 21
                    font.weight: Font.ExtraBold
                    Layout.fillWidth: true
                }
                TfText {
                    visible: (card.opportunity.summary ?? "") !== ""
                    text: card.opportunity.summary ?? ""
                    variant: "secondary"
                    font.weight: Font.Bold
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    Layout.fillWidth: true
                }
            }
            ColumnLayout {
                spacing: -4
                Layout.alignment: Qt.AlignTop
                TfText {
                    text: Theme.percent(card.score)
                    font.pixelSize: card.compact ? 28 : 34
                    font.weight: Font.ExtraBold
                    font.letterSpacing: -0.8
                    color: Theme.scoreColor(card.score)
                    Layout.alignment: Qt.AlignRight
                }
                TfText {
                    text: qsTr("match")
                    variant: "muted"
                    font.weight: Font.Bold
                    Layout.alignment: Qt.AlignRight
                }
            }
        }

        SkillChips {
            Layout.fillWidth: true
            skills: (card.opportunity.keySkills ?? []).map(k => ({ skill: (k.covered ? "✓ " : "") + k.skill, level: 0, covered: k.covered }))
            maxVisible: 3 // the key skills; the rest are behind "+N more"
            expandable: false
            showLevels: false
            toneFor: entry => entry.covered ? "success" : "neutral"
        }

        TfText {
            text: Theme.fitSummary(card.opportunity)
            variant: "muted"
            font.weight: Font.Bold
            Layout.fillWidth: true
        }

        RowLayout {
            visible: !card.compact
            Layout.fillWidth: true
            Layout.topMargin: 2
            spacing: 8
            TfButton {
                visible: card.status === ""
                text: qsTr("Express interest")
                variant: "primary"
                compact: true
                ToolTip.visible: hovered
                ToolTip.delay: 400
                ToolTip.text: qsTr("Tell the host you want to join this team")
                onClicked: card.expressInterest(card.opportunity.id)
            }
            Chip {
                visible: card.status !== ""
                large: true
                text: (card.status === "accepted" ? "✓  " : "") + Theme.interestLabel(card.status)
                tone: Theme.interestTone(card.status)
            }
            TfButton {
                text: qsTr("View")
                compact: true
                onClicked: card.viewOpportunity(card.opportunity.id)
            }
            TfButton {
                text: qsTr("Why this match?")
                variant: "ghost"
                compact: true
                onClicked: card.whyThisMatch(card.opportunity.id)
            }
            Item { Layout.fillWidth: true }
        }
    }

    HoverHandler { id: hover }
    // Compact cards open the opportunity on click.
    TapHandler {
        enabled: card.compact
        onTapped: card.viewOpportunity(card.opportunity.id)
    }
    Accessible.role: Accessible.Button
    Accessible.name: card.opportunity.name ?? ""
    Accessible.onPressAction: card.viewOpportunity(card.opportunity.id)
}

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// The team being assembled for the active project. Size limits, coverage, gaps and the
// finalisation check all come from Backend.currentTeam.
Panel {
    id: panel

    signal explainRequested(string studentId)
    signal notify(string message)

    readonly property var team: Backend.currentTeam
    readonly property int size: team.size ?? 0
    readonly property int minSize: team.minTeamSize ?? 0
    readonly property int maxSize: team.maxTeamSize ?? 0
    readonly property real coverage: team.coverageRatio ?? 0
    readonly property var missing: team.missingSkills ?? []
    readonly property bool canSave: team.canFinalize ?? false
    readonly property var savedForProject: Backend.savedTeams.filter(t => t.requirementId === Backend.currentRequirementId)

    // Member ids before the latest change, so only newly added members animate in (the member
    // list is rebuilt on every change).
    property var previousIds: []
    property var currentIds: []
    onTeamChanged: {
        previousIds = currentIds
        currentIds = team.memberIds ?? []
    }

    title: qsTr("Your team")
    padded: false

    actions: [
        TfButton {
            text: qsTr("Suggest team")
            variant: "secondary"
            compact: true
            ToolTip.visible: hovered
            ToolTip.delay: 500
            ToolTip.text: qsTr("Fill the team automatically with the best-matching people")
            onClicked: if (Backend.suggestTeam()) panel.notify(qsTr("Suggested a team of %1").arg(panel.size))
        },
        TfButton {
            text: qsTr("Clear")
            variant: "ghost"
            compact: true
            enabled: panel.size > 0
            onClicked: Backend.clearTeam()
        }
    ]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Size and coverage at a glance.
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.pad
            Layout.rightMargin: Theme.pad
            spacing: 18

            CoverageRing {
                size: 84
                lineWidth: 8
                value: panel.coverage
                fillColor: panel.coverage >= 1 ? Theme.success : Theme.accent
                caption: qsTr("coverage")
            }
            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                RowLayout {
                    spacing: 6
                    TfText {
                        text: panel.size
                        variant: "number"
                    }
                    TfText {
                        text: qsTr("of %1–%2 members").arg(panel.minSize).arg(panel.maxSize)
                        variant: "muted"
                        font.weight: Font.Bold
                        Layout.alignment: Qt.AlignBottom
                        Layout.bottomMargin: 7
                    }
                }
                // Seats: filled, required and optional.
                Row {
                    spacing: 4
                    Repeater {
                        model: panel.maxSize
                        delegate: Rectangle {
                            required property int index
                            width: 18
                            height: 6
                            radius: 3
                            color: index < panel.size ? (panel.canSave ? Theme.success : Theme.accent)
                                 : index < panel.minSize ? Theme.borderStrong : Theme.border
                            Behavior on color { ColorAnimation { duration: Theme.animNormal } }
                        }
                    }
                }
                TfText {
                    Layout.topMargin: 4
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.Bold
                    color: !panel.canSave ? Theme.textSecondary : panel.missing.length > 0 ? Theme.warning : Theme.success
                    text: !panel.canSave
                          ? ((panel.team.membersNeeded ?? 0) > 0
                             ? (panel.team.membersNeeded === 1 ? qsTr("Add 1 more member to save")
                                                               : qsTr("Add %1 more members to save").arg(panel.team.membersNeeded))
                             : (panel.team.finalizeIssue ?? ""))
                          : panel.missing.length > 0
                            ? qsTr("Ready to save · %1 uncovered").arg(Theme.count(panel.missing.length, qsTr("skill"), qsTr("skills")))
                            : qsTr("Ready to save · every skill covered")
                }
            }
        }

        // Missing skills
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.pad
            Layout.rightMargin: Theme.pad
            Layout.topMargin: 16
            spacing: 8
            TfText {
                text: panel.missing.length > 0 ? qsTr("Missing skills") : qsTr("All required skills covered")
                variant: "label"
                color: panel.missing.length > 0 ? Theme.textMuted : Theme.success
            }
            Flow {
                visible: panel.missing.length > 0
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: panel.missing
                    delegate: Chip {
                        required property string modelData
                        skill: true
                        text: modelData
                        tone: "accent"
                    }
                }
            }
        }

        // Members
        TfText {
            text: qsTr("Members")
            variant: "label"
            Layout.leftMargin: Theme.pad
            Layout.topMargin: 18
            Layout.bottomMargin: 6
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 104
            clip: true
            model: panel.team.members ?? []
            boundsBehavior: Flickable.StopAtBounds
            ScrollIndicator.vertical: ScrollIndicator {}

            delegate: Rectangle {
                id: member
                required property var modelData
                width: ListView.view.width
                implicitHeight: 56
                color: memberHover.hovered ? Theme.surfaceRaised : "transparent"
                Behavior on color { ColorAnimation { duration: Theme.animFast } }
                Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }

                // New members fade and slide in.
                Component.onCompleted: if (panel.previousIds.indexOf(modelData.id) < 0) appear.start()
                ParallelAnimation {
                    id: appear
                    NumberAnimation { target: member; property: "opacity"; from: 0; to: 1; duration: Theme.animNormal }
                    NumberAnimation { target: slide; property: "x"; from: 12; to: 0; duration: Theme.animNormal; easing.type: Easing.OutCubic }
                }
                transform: Translate { id: slide }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.pad
                    anchors.rightMargin: 10
                    spacing: 12
                    Avatar {
                        name: member.modelData.name
                        seed: member.modelData.id
                        size: 38
                    }
                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        TfText {
                            text: member.modelData.name
                            font.weight: Font.ExtraBold
                            Layout.fillWidth: true
                        }
                        TfText {
                            text: member.modelData.skillsOffered.slice().sort((a, b) => b.level - a.level).slice(0, 3)
                                  .map(s => Theme.skill(s.skill)).join(" · ")
                            variant: "muted"
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            Layout.fillWidth: true
                        }
                    }
                    TfButton {
                        text: qsTr("Why this match?")
                        variant: "ghost"
                        compact: true
                        leftPadding: 8
                        rightPadding: 8
                        font.pixelSize: Theme.fontXs
                        opacity: memberHover.hovered ? 1 : 0.7
                        onClicked: panel.explainRequested(member.modelData.id)
                    }
                    TfButton {
                        text: "✕"
                        variant: "ghost"
                        compact: true
                        Layout.preferredWidth: 34
                        Accessible.name: qsTr("Remove %1").arg(member.modelData.name)
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Remove from team")
                        onClicked: Backend.removeFromTeam(member.modelData.id)
                    }
                }
                HoverHandler { id: memberHover }
            }

            header: Item {
                width: ListView.view.width
                height: panel.size === 0 ? 130 : 0
                visible: panel.size === 0
                EmptyState {
                    anchors.centerIn: parent
                    width: parent.width - 40
                    title: qsTr("No members yet")
                    message: qsTr("Add people from Best matches, or click Suggest team.")
                }
            }

            // Saved teams scroll with the members, so the panel never overflows on short windows.
            footer: ColumnLayout {
                width: ListView.view.width
                height: visible ? implicitHeight : 0
                spacing: 10
                visible: panel.savedForProject.length > 0
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
                TfText {
                    text: qsTr("Saved for this project")
                    variant: "label"
                    Layout.leftMargin: Theme.pad
                    Layout.topMargin: 6
                }
                Repeater {
                    model: panel.savedForProject
                    delegate: RowLayout {
                        id: savedRow
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.pad
                        Layout.rightMargin: 10
                        spacing: 10
                        Rectangle {
                            implicitWidth: 22
                            implicitHeight: 22
                            radius: 11
                            color: Theme.successSubtle
                            Text {
                                anchors.centerIn: parent
                                text: "✓"
                                font.pixelSize: 12
                                font.weight: Font.Black
                                color: Theme.success
                            }
                        }
                        ColumnLayout {
                            spacing: 0
                            Layout.fillWidth: true
                            TfText {
                                text: savedRow.modelData.id
                                font.weight: Font.ExtraBold
                                Layout.fillWidth: true
                            }
                            TfText {
                                text: qsTr("%1 · %2 coverage").arg(Theme.count(savedRow.modelData.size ?? 0, qsTr("member"), qsTr("members")))
                                      .arg(Theme.percent(savedRow.modelData.coverageRatio ?? 0))
                                variant: "muted"
                                font.pixelSize: 12
                                Layout.fillWidth: true
                            }
                        }
                        TfButton {
                            text: qsTr("Delete")
                            variant: "danger"
                            compact: true
                            Accessible.name: qsTr("Delete saved team %1").arg(savedRow.modelData.id)
                            // Saving a team writes it to disk, so deleting one does too.
                            onClicked: if (Backend.deleteSavedTeam(savedRow.modelData.id)) Backend.save()
                        }
                    }
                }
                Item { Layout.preferredHeight: 6 }
            }
        }

        // Save
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.pad
            Layout.topMargin: 14
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TfTextField {
                    id: teamName
                    placeholderText: qsTr("Team name")
                    Layout.fillWidth: true
                    onAccepted: if (saveButton.enabled) saveButton.clicked()
                }
                TfButton {
                    id: saveButton
                    text: qsTr("Save team")
                    variant: "primary"
                    enabled: panel.canSave && teamName.text.trim() !== ""
                    ToolTip.visible: hovered && !enabled
                    ToolTip.delay: 300
                    ToolTip.text: !panel.canSave ? qsTr("The team needs more members first") : qsTr("Enter a team name")
                    onClicked: {
                        const name = teamName.text.trim()
                        if (Backend.saveTeam(name)) {
                            teamName.clear()
                            panel.notify(qsTr("Team “%1” saved").arg(name))
                        }
                    }
                }
            }

        }
    }
}

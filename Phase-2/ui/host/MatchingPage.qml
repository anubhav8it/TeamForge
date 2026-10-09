pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Matching: best matches (main column) and the team being built (right). Each row is
// scannable at a glance; the full explanation is behind "Why this match?" (Match insights).
// Ranking and scores come from the backend; this page only shows them and forwards actions.
TfPage {
    id: page

    signal createProject()
    signal explain(string studentId)
    signal notify(string message)

    property string explainedId: ""
    // Ignores a second Add within a moment: after an add the list re-ranks and the next
    // person slides under the pointer, so a double click must not add them too.
    property bool addCooldown: false

    readonly property var project: Backend.currentRequirement
    readonly property bool hasProject: project.id !== undefined
    // "all": the ranking of everyone; "accepted": the people accepted for this project.
    property string source: "all"
    readonly property var ranked: source === "accepted" ? Backend.acceptedCandidates : Backend.rankedCandidates
    readonly property var team: Backend.currentTeam
    readonly property bool teamFull: team.isFull ?? true

    // The strongest required skills a candidate brings, best first. Presentation only:
    // statuses and levels come from the backend's skill breakdown.
    function strongestSkills(result) {
        const covered = result.skillBreakdown.filter(s => s.status === "covered")
        const pool = covered.length > 0 ? covered : result.skillBreakdown.filter(s => s.status === "below")
        return pool.slice().sort((a, b) => (b.fillsGap - a.fillsGap) || (b.level - a.level)).slice(0, 3)
    }

    function add(result) {
        if (addCooldown)
            return
        addCooldown = true
        cooldown.restart()
        if (Backend.addToTeam(result.studentId))
            notify(qsTr("%1 added to your team").arg(result.studentName))
    }

    Timer {
        id: cooldown
        interval: 400
        onTriggered: page.addCooldown = false
    }

    EmptyState {
        visible: !page.hasProject
        anchors.centerIn: parent
        width: 380
        title: qsTr("No active project")
        message: qsTr("Matching ranks participants against a project's required skills.")
        actionText: qsTr("Create project")
        onActionTriggered: page.createProject()
    }

    ColumnLayout {
        visible: page.hasProject
        anchors.fill: parent
        anchors.margins: 24
        anchors.topMargin: 20
        spacing: Theme.gap

        // What we are matching for, and the three steps.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: strip.implicitHeight + 32
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 16
                width: 4
                radius: 2
                color: Theme.typeColor(page.project.type ?? "")
            }

            ColumnLayout {
                id: strip
                anchors.fill: parent
                anchors.margins: 16
                anchors.leftMargin: 32
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    TfText {
                        text: page.project.name ?? ""
                        variant: "section"
                        font.pixelSize: 21
                        font.weight: Font.ExtraBold
                        Layout.maximumWidth: 440
                    }
                    Chip {
                        visible: (page.project.type ?? "") !== ""
                        text: page.project.type ?? ""
                        tone: "tint"
                        tint: Theme.typeColor(page.project.type ?? "")
                    }
                    Item { Layout.fillWidth: true }

                    // Pick people -> check coverage -> save team. Done steps turn green.
                    Repeater {
                        model: [qsTr("Pick people"), qsTr("Check coverage"), qsTr("Save team")]
                        delegate: RowLayout {
                            id: stepItem
                            required property string modelData
                            required property int index
                            readonly property bool done: index === 0 ? (page.team.size ?? 0) > 0
                                                       : index === 1 ? (page.team.canFinalize ?? false)
                                                                     : (page.project.savedTeamCount ?? 0) > 0
                            spacing: 7
                            Rectangle {
                                visible: stepItem.index > 0
                                implicitWidth: 16
                                implicitHeight: 2
                                radius: 1
                                color: Theme.borderStrong
                            }
                            Rectangle {
                                implicitWidth: 24
                                implicitHeight: 24
                                radius: 12
                                color: stepItem.done ? Theme.success : "transparent"
                                border.width: stepItem.done ? 0 : 1.5
                                border.color: Theme.borderStrong
                                Behavior on color { ColorAnimation { duration: Theme.animNormal } }
                                Text {
                                    anchors.centerIn: parent
                                    text: stepItem.done ? "✓" : stepItem.index + 1
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 12
                                    font.weight: Font.ExtraBold
                                    color: stepItem.done ? "#0E2A1E" : Theme.textSecondary
                                }
                            }
                            TfText {
                                text: stepItem.modelData
                                font.pixelSize: Theme.fontSm
                                font.weight: Font.Bold
                                color: stepItem.done ? Theme.text : Theme.textSecondary
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    TfText {
                        text: qsTr("Team of %1–%2").arg(page.project.minTeamSize ?? 0).arg(page.project.maxTeamSize ?? 0)

                        variant: "secondary"
                        font.weight: Font.Bold
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 4
                    }
                    Rectangle {
                        implicitWidth: 1
                        implicitHeight: 18
                        color: Theme.borderStrong
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 5
                        Layout.leftMargin: 4
                        Layout.rightMargin: 4
                    }
                    TfText {
                        text: qsTr("Needs")
                        variant: "label"
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 5
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: page.project.requiredSkills ?? []
                            delegate: Chip {
                                required property var modelData
                                readonly property bool covered: (page.team.coveredSkills ?? []).indexOf(modelData.skill) >= 0
                                text: (covered ? "✓ " : "") + Theme.skill(modelData.skill) + "  " + modelData.minLevel + "+"
                                tone: covered ? "success" : "neutral"
                                ToolTip.visible: chipHover.hovered
                                ToolTip.delay: 400
                                ToolTip.text: covered ? qsTr("Covered by your team")
                                                      : qsTr("Needs someone at level %1 or higher").arg(modelData.minLevel)
                                HoverHandler { id: chipHover }
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.gap

            // Best matches
            Panel {
                title: page.source === "accepted" ? qsTr("Accepted applicants") : qsTr("Best matches")
                subtitle: page.source === "accepted"
                          ? qsTr("People you accepted for %1, best fit first").arg(page.project.name ?? "")
                          : qsTr("Top %1 of %2 candidates").arg(page.ranked.length).arg(page.project.candidateCount ?? 0)
                padded: false
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 480

                actions: [
                Segmented {
                    options: [{ text: qsTr("Everyone"), value: "all" },
                              { text: qsTr("Accepted %1").arg(page.project.interest?.accepted ?? 0), value: "accepted" }]
                    currentValue: page.source
                    onActivated: value => page.source = value
                },
                Segmented {
                    visible: page.source === "all"
                    options: [{ text: qsTr("Top 10"), value: 10 }, { text: "25", value: 25 }, { text: "50", value: 50 }]
                    currentValue: Backend.topK
                    onActivated: value => Backend.topK = value
                    ToolTip.visible: topHover.hovered
                    ToolTip.delay: 500
                    ToolTip.text: qsTr("How many matches to show")
                    HoverHandler { id: topHover }
                }
                ]

                ListView {
                    id: rankList
                    anchors.fill: parent
                    clip: true
                    model: page.ranked
                    reuseItems: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Rectangle {
                        id: row
                        required property var modelData
                        required property int index
                        readonly property bool explained: page.explainedId === modelData.studentId
                        Accessible.role: Accessible.ListItem
                        Accessible.name: modelData.studentName
                        width: ListView.view.width
                        implicitHeight: 84
                        color: explained ? Theme.selection : rowHover.hovered ? Theme.surfaceRaised : "transparent"
                        Behavior on color { ColorAnimation { duration: Theme.animFast } }
                        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }
                        Rectangle {
                            visible: row.explained
                            width: 4
                            height: parent.height
                            color: Theme.accent
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.pad
                            anchors.rightMargin: Theme.pad
                            spacing: 14

                            Avatar {
                                name: row.modelData.studentName
                                seed: row.modelData.studentId
                                size: 48
                                selected: row.explained
                            }
                            ColumnLayout {
                                spacing: 7
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                TfText {
                                    text: row.modelData.studentName
                                    font.pixelSize: Theme.fontMd + 1
                                    font.weight: Font.ExtraBold
                                    Layout.fillWidth: true
                                }
                                // One clipped line: chips that do not fit wrap out of view instead of
                                // running under the score.
                                Flow {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    clip: true
                                    spacing: 6
                                    Repeater {
                                        model: page.strongestSkills(row.modelData)
                                        delegate: Chip {
                                            required property var modelData
                                            skill: true
                                            text: modelData.skill + "  " + modelData.level
                                            tone: modelData.status !== "covered" ? "warning" : modelData.fillsGap ? "success" : "neutral"
                                        }
                                    }
                                }
                            }
                            TfText {
                                text: Theme.percent(row.modelData.score)
                                variant: "score"
                                font.pixelSize: 28
                                color: Theme.scoreColor(row.modelData.score)
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: 78
                                Layout.fillWidth: false
                            }
                            TfButton {
                                text: qsTr("Why this match?")
                                variant: "secondary"
                                compact: true
                                onClicked: page.explain(row.modelData.studentId)
                            }
                            Chip {
                                visible: row.modelData.isMember === true
                                text: qsTr("In team")
                                tone: "accent"
                                Layout.preferredWidth: 78
                            }
                            TfButton {
                                visible: row.modelData.isMember !== true
                                text: qsTr("+ Add")
                                variant: "primary"
                                compact: true
                                enabled: !page.teamFull && !page.addCooldown
                                Layout.preferredWidth: 78
                                Accessible.name: qsTr("Add %1").arg(row.modelData.studentName)
                                ToolTip.visible: hovered && page.teamFull
                                ToolTip.text: qsTr("Your team is full")
                                onClicked: page.add(row.modelData)
                            }
                        }
                        HoverHandler { id: rowHover }
                    }

                    EmptyState {
                        anchors.centerIn: parent
                        width: parent.width - 60
                        visible: page.ranked.length === 0
                        title: page.source === "accepted" ? qsTr("Nobody accepted yet")
                             : (page.team.size ?? 0) > 0 ? qsTr("No more candidates") : qsTr("No candidates yet")
                        message: page.source === "accepted" ? qsTr("Accept applicants on the Applicants page; they appear here for team building.")
                                                           : qsTr("Nobody else offers any of this project's required skills.")
                    }
                }
            }

            TeamBuilderPanel {
                Layout.fillHeight: true
                Layout.preferredWidth: page.width < 1150 ? 350 : 390
                Layout.minimumWidth: 330
                onExplainRequested: studentId => page.explain(studentId)
                onNotify: message => page.notify(message)
            }
        }
    }
}

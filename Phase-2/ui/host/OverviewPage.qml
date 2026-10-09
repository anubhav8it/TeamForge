pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Host overview: active project and its required skills, participant pool, best matches,
// team progress and recent projects. Every figure comes from the backend's read models.
TfPage {
    id: page

    signal startMatching()
    signal reviewApplicants()
    signal editProject()
    signal createProject()
    signal explain(string studentId)
    signal notify(string message)

    readonly property var project: Backend.currentRequirement
    readonly property bool hasProject: project.id !== undefined
    readonly property var team: Backend.currentTeam
    readonly property int participantCount: Backend.students.length
    readonly property var requiredSkills: project.requiredSkills ?? []
    readonly property var topMatches: Backend.rankedCandidates.slice(0, 3)
    // The required skill with the fewest qualified people: the likely bottleneck.
    readonly property var scarcest: requiredSkills.length > 0
                                    ? requiredSkills.reduce((a, b) => b.qualifiedCount < a.qualifiedCount ? b : a) : null
    readonly property bool wide: width >= 1100
    // Ignores a second Add while the list re-ranks under the pointer.
    property bool addCooldown: false

    Timer {
        id: addTimer
        interval: 400
        onTriggered: page.addCooldown = false
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: Theme.gap

            Item { Layout.preferredHeight: 4 }

            EmptyState {
                visible: !page.hasProject
                title: qsTr("No project yet")
                message: qsTr("Create a project with the skills it needs to start matching.")
                actionText: qsTr("Create project")
                Layout.fillWidth: true
                Layout.topMargin: 80
                onActionTriggered: page.createProject()
            }

            // Row 1: active project (hero) + participant pool.
            RowLayout {
                visible: page.hasProject
                spacing: Theme.gap
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    implicitHeight: hero.implicitHeight + 48
                    radius: Theme.radius
                    color: Theme.surface
                    border.color: Theme.border

                    // Accent wash in the project's type colour.
                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radius
                        opacity: 0.10
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Theme.typeColor(page.project.type ?? "") }
                            GradientStop { position: 0.7; color: "transparent" }
                        }
                    }

                    ColumnLayout {
                        id: hero
                        anchors.fill: parent
                        anchors.margins: 24
                        spacing: 14

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            TfText {
                                text: qsTr("Active project")
                                variant: "eyebrow"
                            }
                            Chip {
                                visible: (page.project.type ?? "") !== ""
                                text: page.project.type ?? ""
                                tone: "tint"
                                tint: Theme.typeColor(page.project.type ?? "")
                            }
                            Item { Layout.fillWidth: true }
                        }
                        TfText {
                            text: page.project.name ?? ""
                            font.pixelSize: 32
                            font.weight: Font.ExtraBold
                            font.letterSpacing: -0.8
                            Layout.fillWidth: true
                        }
                        TfText {
                            visible: (page.project.summary ?? "") !== ""
                            text: page.project.summary ?? ""
                            font.pixelSize: Theme.fontLg
                            font.weight: Font.Bold
                            color: Theme.textSecondary
                            Layout.fillWidth: true
                            Layout.topMargin: -8
                        }
                        TfText {
                            text: qsTr("Team of %1–%2").arg(page.project.minTeamSize ?? 0).arg(page.project.maxTeamSize ?? 0)
                                  + "  ·  " + Theme.count(page.requiredSkills.length, qsTr("required skill"), qsTr("required skills"))
                            variant: "secondary"
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                        }

                        TfText {
                            text: qsTr("Required skills")
                            variant: "label"
                            Layout.topMargin: 4
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            Repeater {
                                model: page.requiredSkills
                                delegate: Chip {
                                    required property var modelData
                                    readonly property bool covered: (page.team.coveredSkills ?? []).indexOf(modelData.skill) >= 0
                                    large: true
                                    text: (covered ? "✓ " : "") + Theme.skill(modelData.skill) + "  " + modelData.minLevel + "+"
                                    tone: covered ? "success" : "neutral"
                                    ToolTip.visible: skillHover.hovered
                                    ToolTip.delay: 300
                                    ToolTip.text: qsTr("%1 people qualify (level %2 or higher)")
                                                  .arg(modelData.qualifiedCount).arg(modelData.minLevel)
                                    HoverHandler { id: skillHover }
                                }
                            }
                        }

                        RowLayout {
                            Layout.topMargin: 6
                            spacing: 10
                            TfButton {
                                text: qsTr("Find teammates")
                                variant: "primary"
                                onClicked: page.startMatching()
                            }
                            TfButton {
                                text: qsTr("Edit project")
                                variant: "secondary"
                                onClicked: page.editProject()
                            }
                        }
                    }
                }

                Panel {
                    title: qsTr("Participant pool")
                    Layout.preferredWidth: 300
                    Layout.minimumWidth: 270
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 14

                        ColumnLayout {
                            spacing: -2
                            TfText {
                                text: page.participantCount
                                font.pixelSize: 44
                                font.weight: Font.ExtraBold
                                font.letterSpacing: -1.2
                            }
                            TfText {
                                text: qsTr("participants · %1").arg(Theme.count(Backend.skills.length, qsTr("skill"), qsTr("skills")))
                                variant: "muted"
                                font.weight: Font.Bold
                            }
                        }
                        ColumnLayout {
                            spacing: 6
                            Layout.fillWidth: true
                            RowLayout {
                                Layout.fillWidth: true
                                TfText { text: qsTr("Can help this project"); variant: "secondary"; font.weight: Font.Bold; Layout.fillWidth: true }
                                TfText { text: page.project.candidateCount ?? 0; font.weight: Font.ExtraBold; color: Theme.info }
                            }
                            MeterBar {
                                Layout.fillWidth: true
                                value: page.participantCount > 0 ? (page.project.candidateCount ?? 0) / page.participantCount : 0
                                fillColor: Theme.info
                            }
                        }
                        ColumnLayout {
                            visible: page.scarcest !== null
                            spacing: 2
                            Layout.fillWidth: true
                            TfText { text: qsTr("Hardest skill to fill"); variant: "label" }
                            RowLayout {
                                spacing: 8
                                Chip {
                                    skill: true
                                    text: page.scarcest?.skill ?? ""
                                    tone: "warning"
                                }
                                TfText {
                                    text: qsTr("%1 qualified").arg(page.scarcest?.qualifiedCount ?? 0)
                                    variant: "secondary"
                                    font.weight: Font.Bold
                                }
                            }
                        }
                        // Interest in this project.
                        ColumnLayout {
                            spacing: 6
                            Layout.fillWidth: true
                            TfText { text: qsTr("Applicants"); variant: "label" }
                            RowLayout {
                                spacing: 8
                                Chip {
                                    text: qsTr("%1 new").arg((page.project.interest?.interested ?? 0) + (page.project.interest?.under_review ?? 0))
                                    tone: "info"
                                }
                                Chip {
                                    text: qsTr("%1 accepted").arg(page.project.interest?.accepted ?? 0)
                                    tone: "success"
                                }
                            }
                            TfButton {
                                text: qsTr("Review applicants")
                                compact: true
                                onClicked: page.reviewApplicants()
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }

            // Row 2: best matches + team progress.
            RowLayout {
                visible: page.hasProject
                spacing: Theme.gap
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24

                Panel {
                    title: qsTr("Best matches")
                    subtitle: qsTr("Top picks for your current team")
                    padded: false
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    actions: TfButton {
                        text: qsTr("See all")
                        variant: "ghost"
                        compact: true
                        onClicked: page.startMatching()
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0
                        Repeater {
                            model: page.topMatches
                            delegate: Rectangle {
                                id: matchRow
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                implicitHeight: 70
                                color: matchHover.hovered ? Theme.surfaceRaised : "transparent"
                                Behavior on color { ColorAnimation { duration: Theme.animFast } }
                                Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.pad
                                    anchors.rightMargin: Theme.pad
                                    spacing: 14
                                    Avatar {
                                        name: matchRow.modelData.studentName
                                        seed: matchRow.modelData.studentId
                                        size: 44
                                    }
                                    ColumnLayout {
                                        spacing: 5
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 1
                                        TfText {
                                            text: matchRow.modelData.studentName
                                            font.pixelSize: Theme.fontMd
                                            font.weight: Font.ExtraBold
                                            Layout.fillWidth: true
                                        }
                                        Flow {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 26
                                            clip: true
                                            spacing: 6
                                            Repeater {
                                                model: matchRow.modelData.skillBreakdown.filter(s => s.status === "covered")
                                                       .sort((a, b) => b.level - a.level).slice(0, 3)
                                                delegate: Chip {
                                                    required property var modelData
                                                    skill: true
                                                    text: modelData.skill + "  " + modelData.level
                                                    tone: "success"
                                                }
                                            }
                                        }
                                    }
                                    TfText {
                                        text: Theme.percent(matchRow.modelData.score)
                                        variant: "score"
                                        color: Theme.scoreColor(matchRow.modelData.score)
                                        horizontalAlignment: Text.AlignRight
                                        Layout.preferredWidth: 72
                                        Layout.fillWidth: false
                                    }
                                    TfButton {
                                        text: qsTr("Why this match?")
                                        variant: "secondary"
                                        compact: true
                                        onClicked: page.explain(matchRow.modelData.studentId)
                                    }
                                    TfButton {
                                        text: qsTr("+ Add")
                                        variant: "primary"
                                        compact: true
                                        enabled: !(page.team.isFull ?? true) && !page.addCooldown
                                        Accessible.name: qsTr("Add %1").arg(matchRow.modelData.studentName)
                                        onClicked: {
                                            page.addCooldown = true
                                            addTimer.restart()
                                            if (Backend.addToTeam(matchRow.modelData.studentId))
                                                page.notify(qsTr("%1 added to your team").arg(matchRow.modelData.studentName))
                                        }
                                    }
                                }
                                HoverHandler { id: matchHover }
                            }
                        }
                        TfText {
                            visible: page.topMatches.length === 0
                            text: qsTr("No more candidates for this project.")
                            variant: "muted"
                            Layout.margins: Theme.pad
                        }
                        Item { Layout.preferredHeight: 6 }
                    }
                }

                Panel {
                    title: qsTr("Team progress")
                    subtitle: (page.team.size ?? 0) > 0 ? Theme.count(page.team.size, qsTr("member so far"), qsTr("members so far"))
                                                        : qsTr("Nobody added yet")
                    Layout.preferredWidth: 380
                    Layout.minimumWidth: 320
                    Layout.fillHeight: true

                    actions: TfButton {
                        text: qsTr("Open team")
                        variant: "ghost"
                        compact: true
                        onClicked: page.startMatching()
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 14

                        RowLayout {
                            spacing: 18
                            CoverageRing {
                                size: 92
                                value: page.team.coverageRatio ?? 0
                                fillColor: (page.team.coverageRatio ?? 0) >= 1 ? Theme.success : Theme.accent
                                caption: qsTr("coverage")
                            }
                            ColumnLayout {
                                spacing: 4
                                Layout.fillWidth: true
                                RowLayout {
                                    spacing: 6
                                    TfText { text: page.team.size ?? 0; variant: "number" }
                                    TfText {
                                        text: qsTr("of %1–%2").arg(page.team.minTeamSize ?? 0).arg(page.team.maxTeamSize ?? 0)
                                        variant: "muted"
                                        font.weight: Font.Bold
                                        Layout.alignment: Qt.AlignBottom
                                        Layout.bottomMargin: 7
                                    }
                                }
                                Row {
                                    visible: (page.team.size ?? 0) > 0
                                    spacing: -8
                                    Repeater {
                                        model: page.team.members ?? []
                                        // A ring in the card colour keeps overlapping avatars apart.
                                        delegate: Rectangle {
                                            id: stacked
                                            required property var modelData
                                            width: 38
                                            height: 38
                                            radius: 19
                                            color: Theme.surface
                                            Avatar {
                                                anchors.centerIn: parent
                                                name: stacked.modelData.name
                                                seed: stacked.modelData.id
                                                size: 34
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        TfText {
                            text: (page.team.missingSkills ?? []).length > 0 ? qsTr("Still missing") : qsTr("Every required skill is covered")
                            variant: "label"
                            color: (page.team.missingSkills ?? []).length > 0 ? Theme.textMuted : Theme.success
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 6
                            Repeater {
                                model: page.team.missingSkills ?? []
                                delegate: Chip {
                                    required property string modelData
                                    skill: true
                                    text: modelData
                                    tone: "accent"
                                }
                            }
                        }
                        // Interest in this project.
                        ColumnLayout {
                            spacing: 6
                            Layout.fillWidth: true
                            TfText { text: qsTr("Applicants"); variant: "label" }
                            RowLayout {
                                spacing: 8
                                Chip {
                                    text: qsTr("%1 new").arg((page.project.interest?.interested ?? 0) + (page.project.interest?.under_review ?? 0))
                                    tone: "info"
                                }
                                Chip {
                                    text: qsTr("%1 accepted").arg(page.project.interest?.accepted ?? 0)
                                    tone: "success"
                                }
                            }
                            TfButton {
                                text: qsTr("Review applicants")
                                compact: true
                                onClicked: page.reviewApplicants()
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }

            // Row 3: recent projects as cards.
            ColumnLayout {
                visible: page.hasProject
                spacing: 12
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    TfText { text: qsTr("Recent projects"); variant: "section"; Layout.fillWidth: true }
                    TfButton {
                        text: qsTr("+ New project")
                        variant: "secondary"
                        compact: true
                        onClicked: page.createProject()
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: page.wide ? 4 : 3
                    columnSpacing: 12
                    rowSpacing: 12

                    Repeater {
                        model: Backend.recentProjects.slice(0, page.wide ? 8 : 6)
                        delegate: Rectangle {
                            id: card
                            required property var modelData
                            readonly property bool active: modelData.id === Backend.currentRequirementId
                            readonly property color tone: Theme.typeColor(modelData.type ?? "")
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.name
                            Accessible.onPressAction: Backend.currentRequirementId = modelData.id
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            implicitHeight: 112
                            radius: Theme.radius
                            color: cardHover.hovered ? Theme.surfaceRaised : Theme.surface
                            border.width: active ? 2 : 1
                            border.color: active ? Theme.accent : cardHover.hovered ? Theme.borderStrong : Theme.border
                            Behavior on color { ColorAnimation { duration: Theme.animFast } }
                            scale: cardTap.pressed ? 0.98 : 1
                            Behavior on scale { NumberAnimation { duration: 90 } }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Rectangle {
                                        implicitWidth: 8
                                        implicitHeight: 8
                                        radius: 4
                                        color: card.tone
                                    }
                                    TfText {
                                        text: card.modelData.type || qsTr("Project")
                                        variant: "muted"
                                        font.pixelSize: 12
                                        font.weight: Font.Bold
                                        color: card.tone
                                        Layout.fillWidth: true
                                    }
                                    TfText {
                                        visible: card.active
                                        text: qsTr("Active")
                                        font.pixelSize: 12
                                        font.weight: Font.ExtraBold
                                        color: Theme.accentHover
                                    }
                                }
                                TfText {
                                    text: card.modelData.name
                                    font.pixelSize: Theme.fontMd
                                    font.weight: Font.ExtraBold
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    Layout.fillWidth: true
                                }
                                Item { Layout.fillHeight: true }
                                TfText {
                                    text: qsTr("%1–%2 members · %3").arg(card.modelData.minTeamSize).arg(card.modelData.maxTeamSize)
                                          .arg(Theme.count(card.modelData.requiredSkills.length, qsTr("skill"), qsTr("skills")))
                                          + (card.modelData.savedTeamCount > 0
                                             ? " · " + Theme.count(card.modelData.savedTeamCount, qsTr("saved team"), qsTr("saved teams")) : "")
                                    variant: "muted"
                                    font.pixelSize: Theme.fontXs
                                    Layout.fillWidth: true
                                }
                            }
                            HoverHandler {
                                id: cardHover
                                cursorShape: Qt.PointingHandCursor
                            }
                            TapHandler {
                                id: cardTap
                                onTapped: Backend.currentRequirementId = card.modelData.id
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 20 }
        }
    }
}

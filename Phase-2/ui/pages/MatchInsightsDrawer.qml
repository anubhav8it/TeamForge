pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// "Why this match?" -> Match insights. Two uses, both scored by the backend:
//  - a host's candidate against the current team (Backend.evaluateCandidate), with Add/Remove;
//  - a participant's fit with one project (Backend.evaluateFit), read-only.
Drawer {
    id: drawer

    signal notify(string message)

    property string studentId
    property string requirementId   // set for a participant's fit, empty for a team candidate
    property var result: ({})
    property var student: ({})
    property bool showDefinitions: false
    property bool busy: false       // guards Add/Remove against a double click

    readonly property bool fitMode: requirementId !== ""
    readonly property bool hasResult: result.studentId !== undefined
    readonly property var breakdown: result.skillBreakdown ?? []
    readonly property int coveredCount: breakdown.filter(s => s.status === "covered").length
    readonly property bool weighted: (result.factors ?? []).some(f => f.contribution !== undefined)
    readonly property var project: fitMode ? (Backend.requirements.find(r => r.id === requirementId) ?? ({}))
                                           : Backend.currentRequirement

    function showCandidate(id) {
        requirementId = ""
        studentId = id
        refresh()
        open()
    }
    function showFit(id, projectId) {
        requirementId = projectId
        studentId = id
        refresh()
        open()
    }
    function refresh() {
        if (studentId === "") {
            result = {}
            return
        }
        result = fitMode ? Backend.evaluateFit(studentId, requirementId) : Backend.evaluateCandidate(studentId)
        student = Backend.students.find(s => s.id === studentId) ?? ({})
    }

    parent: Overlay.overlay
    edge: Qt.RightEdge
    // One computed width for the drawer and its content, so the drawer's implicit size never
    // depends on its own width or wrapped text (binding loop).
    readonly property real panelWidth: Math.min(520, Math.max(430, (parent?.width ?? 1200) * 0.38))
    width: panelWidth
    height: parent.height
    contentWidth: panelWidth
    modal: true
    dim: true
    padding: 0

    Connections {
        target: Backend
        function onSelectionChanged() { if (drawer.opened && !drawer.fitMode) drawer.refresh() }
        function onProfileChanged() { if (drawer.opened && drawer.fitMode) drawer.refresh() }
    }
    Timer {
        id: cooldown
        interval: 400
        onTriggered: drawer.busy = false
    }

    Overlay.modal: Rectangle { color: "#8C0B0D11" }

    background: Rectangle {
        color: Theme.surface
        Rectangle { width: 1; height: parent.height; color: Theme.borderStrong }
    }

    contentItem: ScrollView {
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 22

            // Header
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 24
                Layout.bottomMargin: 0
                spacing: 16
                Avatar {
                    name: drawer.result.studentName ?? ""
                    seed: drawer.result.studentId ?? ""
                    size: 60
                    highlighted: drawer.result.isMember ?? false
                    Layout.alignment: Qt.AlignTop
                }
                ColumnLayout {
                    spacing: 3
                    Layout.fillWidth: true
                    TfText {
                        text: qsTr("Match insights")
                        variant: "eyebrow"
                    }
                    TfText {
                        text: drawer.result.studentName ?? ""
                        font.pixelSize: 22
                        font.weight: Font.ExtraBold
                        Layout.fillWidth: true
                    }
                    TfText {
                        text: (drawer.fitMode ? qsTr("Fit with %1") : drawer.result.isMember ? qsTr("In your team · %1")
                                                                                             : qsTr("If added to your team · %1"))
                              .arg(drawer.project.name ?? "")
                        variant: "muted"
                        Layout.fillWidth: true
                    }
                }
                TfButton {
                    text: "✕"
                    variant: "ghost"
                    compact: true
                    font.pixelSize: 15
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: 36
                    Accessible.name: qsTr("Close match insights")
                    onClicked: drawer.close()
                }
            }

            // Score + action
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                implicitHeight: scoreColumn.implicitHeight + 36
                radius: Theme.radius
                color: Theme.surfaceRaised
                border.color: Theme.border

                ColumnLayout {
                    id: scoreColumn
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12
                    RowLayout {
                        spacing: 18
                        Layout.fillWidth: true
                        ColumnLayout {
                            spacing: -4
                            TfText {
                                text: Theme.percent(drawer.result.score ?? 0)
                                font.pixelSize: 46
                                font.weight: Font.ExtraBold
                                font.letterSpacing: -1
                                color: Theme.scoreColor(drawer.result.score ?? 0)
                            }
                            TfText {
                                text: qsTr("match score")
                                variant: "muted"
                                font.weight: Font.Bold
                            }
                        }
                        ColumnLayout {
                            spacing: 6
                            Layout.fillWidth: true
                            TfText {
                                text: qsTr("Meets %1 of %2 required skills").arg(drawer.coveredCount).arg(drawer.breakdown.length)
                                font.weight: Font.Bold
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            MeterBar {
                                Layout.fillWidth: true
                                value: drawer.result.score ?? 0
                                fillColor: Theme.scoreColor(drawer.result.score ?? 0)
                            }
                        }
                    }
                    TfButton {
                        visible: !drawer.fitMode && (Backend.workspace === "host" || Backend.workspace === "developer")
                        Layout.fillWidth: true
                        text: drawer.result.isMember ? qsTr("Remove from team") : qsTr("+ Add to team")
                        variant: drawer.result.isMember ? "secondary" : "primary"
                        enabled: !drawer.busy && ((drawer.result.isMember ?? false) || !(Backend.currentTeam.isFull ?? true))
                        onClicked: {
                            drawer.busy = true
                            cooldown.restart()
                            const name = drawer.result.studentName ?? ""
                            if (drawer.result.isMember) {
                                if (Backend.removeFromTeam(drawer.studentId))
                                    drawer.notify(qsTr("%1 removed from your team").arg(name))
                            } else if (Backend.addToTeam(drawer.studentId)) {
                                drawer.notify(qsTr("%1 added to your team").arg(name))
                            }
                        }
                    }
                    TfText {
                        visible: !drawer.fitMode && !(drawer.result.isMember ?? false) && (Backend.currentTeam.isFull ?? false)
                        text: qsTr("Your team is full. Remove someone to add this person.")
                        variant: "muted"
                        Layout.fillWidth: true
                    }
                }
            }

            // Factors
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    TfText {
                        text: qsTr("How the score is built")
                        variant: "heading"
                        font.pixelSize: Theme.fontLg
                        Layout.fillWidth: true
                    }
                    TfButton {
                        text: drawer.showDefinitions ? qsTr("Hide details") : qsTr("Show details")
                        variant: "ghost"
                        compact: true
                        onClicked: drawer.showDefinitions = !drawer.showDefinitions
                    }
                }
                TfText {
                    visible: !drawer.weighted
                    text: qsTr("Coverage-only baseline: the score is skill fit alone. The other factors are shown for reference.")
                    variant: "muted"
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                    Layout.fillWidth: true
                }
                Repeater {
                    model: drawer.result.factors ?? []
                    delegate: ColumnLayout {
                        id: factor
                        required property var modelData
                        required property int index
                        readonly property color tone: Theme.factorColors[index]
                        readonly property var definition: Backend.factorDefinitions[index]
                        spacing: 6
                        Layout.fillWidth: true
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Rectangle {
                                implicitWidth: 10
                                implicitHeight: 10
                                radius: 5
                                color: factor.tone
                            }
                            TfText {
                                text: factor.modelData.shortLabel
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.ExtraBold
                            }
                            TfText {
                                // "Coverage · 40%"; just the weight when the report's term is the same word.
                                text: ((factor.definition?.label ?? "") !== factor.modelData.shortLabel
                                       ? (factor.definition?.label ?? "") + " · " : "") + Theme.percent(factor.definition?.weight ?? 0)
                                variant: "muted"
                                font.weight: Font.Bold
                                Layout.fillWidth: true
                            }
                            TfText {
                                text: drawer.weighted ? qsTr("+%1").arg(((factor.modelData.contribution ?? 0) * 100).toFixed(1))
                                                      : factor.modelData.value.toFixed(2)
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.ExtraBold
                                color: factor.tone
                            }
                        }
                        MeterBar {
                            Layout.fillWidth: true
                            value: factor.modelData.value
                            fillColor: factor.tone
                        }
                        TfText {
                            visible: drawer.showDefinitions
                            text: (factor.definition?.description ?? "") + "  " + qsTr("Value %1 of 1.").arg(factor.modelData.value.toFixed(2))
                            variant: "muted"
                            wrapMode: Text.WordWrap
                            elide: Text.ElideNone
                            Layout.fillWidth: true
                        }
                    }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.borderStrong; visible: drawer.weighted }
                RowLayout {
                    visible: drawer.weighted
                    Layout.fillWidth: true
                    TfText { text: qsTr("Total"); font.weight: Font.ExtraBold; Layout.fillWidth: true }
                    TfText {
                        text: qsTr("%1 of 100").arg(((drawer.result.score ?? 0) * 100).toFixed(1))
                        font.pixelSize: Theme.fontLg
                        font.weight: Font.ExtraBold
                        color: Theme.scoreColor(drawer.result.score ?? 0)
                    }
                }
            }

            // Skill by skill
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.bottomMargin: 28
                spacing: 0
                TfText {
                    text: qsTr("Required skills")
                    variant: "heading"
                    font.pixelSize: Theme.fontLg
                    Layout.bottomMargin: 10
                }
                Repeater {
                    model: drawer.breakdown
                    delegate: Rectangle {
                        id: skillRow
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 48
                        color: "transparent"
                        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }
                        RowLayout {
                            anchors.fill: parent
                            spacing: 12
                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                TfText {
                                    text: Theme.skill(skillRow.modelData.skill)
                                    font.weight: Font.ExtraBold
                                    font.letterSpacing: 0.4
                                    Layout.fillWidth: true
                                }
                                TfText {
                                    text: skillRow.modelData.level > 0
                                          ? qsTr("Level %1 · needs %2").arg(skillRow.modelData.level).arg(skillRow.modelData.minLevel)
                                          : qsTr("Needs level %1").arg(skillRow.modelData.minLevel)
                                    variant: "muted"
                                    font.pixelSize: Theme.fontXs
                                }
                            }
                            LevelPips {
                                level: skillRow.modelData.level
                                minLevel: skillRow.modelData.minLevel
                            }
                            Chip {
                                Layout.preferredWidth: 118
                                text: skillRow.modelData.status === "covered"
                                      ? (drawer.fitMode ? qsTr("Meets it") : skillRow.modelData.fillsGap ? qsTr("Fills a gap") : qsTr("Team has it"))
                                      : skillRow.modelData.status === "below" ? qsTr("Below minimum") : qsTr("Not offered")
                                tone: skillRow.modelData.status === "covered"
                                      ? (drawer.fitMode || skillRow.modelData.fillsGap ? "success" : "neutral")
                                      : skillRow.modelData.status === "below" ? "warning" : "muted"
                            }
                        }
                    }
                }
            }
        }
    }
}

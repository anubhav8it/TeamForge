pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// The developer's workspace controls: preview the product as any participant or as the host,
// manage the local data (save, reload, reset to the seed data), and, folded away under System
// tools, the technical checks: validation, the skill structures, persistence, matching, the
// latest error and the activity log. Every action is a backend call that also checks the
// developer workspace.
TfPage {
    id: page

    signal notify(string message)

    readonly property var status: Backend.systemStatus
    readonly property var index: status.index ?? ({})
    readonly property var error: Backend.lastErrorDetail
    property var dataResult: ({})
    property var validateResult: ({})
    property var indexResult: ({})
    property var persistenceResult: ({})
    property var matchingResult: ({})
    property string logLevel: ""
    property string logQuery: ""
    readonly property var logEntries: Backend.logEntries.filter(e => (logLevel === "" || e.level === logLevel)
        && (logQuery === "" || e.message.toLowerCase().includes(logQuery) || e.category.includes(logQuery)))
    readonly property bool twoColumns: width >= 1000
    property bool showTools: false
    property string participantQuery: ""
    // The demo participant first, then a few others; or search results.
    readonly property var participantChoices: {
        const demoId = Backend.demoIdentities[0]?.id ?? ""
        if (participantQuery.trim() === "")
            return Backend.students.filter(s => s.id === demoId).concat(Backend.students.filter(s => s.id !== demoId).slice(0, 3))
        return (Backend.queryParticipants({ query: participantQuery }).rows ?? []).slice(0, 5)
    }

    component Field: RowLayout {
        id: field
        property string label
        property string value
        property color valueColor: Theme.text
        spacing: 12
        Layout.fillWidth: true
        TfText { text: field.label; variant: "muted"; font.weight: Font.Bold; Layout.preferredWidth: 120; Layout.alignment: Qt.AlignTop }
        TfText {
            text: field.value
            font.weight: Font.Bold
            color: field.valueColor
            wrapMode: Text.Wrap
            elide: Text.ElideNone
            Layout.fillWidth: true
        }
    }
    component Section: Panel {
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.alignment: Qt.AlignTop
    }

    ConfirmDialog {
        id: confirmReset
        onConfirmed: {
            page.dataResult = Backend.resetSampleData()
            if (page.dataResult.ok)
                page.notify(qsTr("Seed data restored"))
        }
    }
    ConfirmDialog {
        id: confirmReload
        onConfirmed: if (Backend.reload()) page.notify(qsTr("Reloaded from disk"))
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width - 48
            x: 24
            spacing: Theme.gap

            Item { Layout.preferredHeight: 4 }

            // Preview the product.
            GridLayout {
                Layout.fillWidth: true
                columns: page.twoColumns ? 2 : 1
                columnSpacing: Theme.gap
                rowSpacing: Theme.gap

                Section {
                    title: qsTr("Preview participant")
                    subtitle: qsTr("Enter the participant workspace as anyone in the database")
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 6
                        TfTextField {
                            placeholderText: qsTr("Search participants by name, id or skill")
                            Layout.fillWidth: true
                            Layout.bottomMargin: 6
                            onTextChanged: page.participantQuery = text
                        }
                        Repeater {
                            model: page.participantChoices
                            delegate: Rectangle {
                                id: choice
                                required property var modelData
                                readonly property bool demo: modelData.id === (Backend.demoIdentities[0]?.id ?? "")
                                Layout.fillWidth: true
                                implicitHeight: 54
                                radius: Theme.radiusSmall
                                color: choiceHover.hovered ? Theme.surfaceRaised : "transparent"
                                Behavior on color { ColorAnimation { duration: Theme.animFast } }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 12
                                    Avatar { name: choice.modelData.name; seed: choice.modelData.id; size: 36 }
                                    ColumnLayout {
                                        spacing: 0
                                        Layout.fillWidth: true
                                        TfText { text: choice.modelData.name; font.weight: Font.ExtraBold; Layout.fillWidth: true }
                                        TfText { text: choice.modelData.id + "  ·  " + (choice.modelData.program ?? ""); variant: "muted"; Layout.fillWidth: true }
                                    }
                                    Chip { visible: choice.demo; text: qsTr("Demo Participant"); tone: "info" }
                                    TfButton {
                                        text: qsTr("Preview")
                                        variant: choice.demo ? "primary" : "secondary"
                                        compact: true
                                        Accessible.name: qsTr("Preview as %1").arg(choice.modelData.name)
                                        onClicked: Backend.previewWorkspace("participant", choice.modelData.id)
                                    }
                                }
                                HoverHandler { id: choiceHover }
                            }
                        }
                    }
                }

                Section {
                    title: qsTr("Preview host")
                    subtitle: qsTr("Enter the host workspace with a project open")
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 10
                        RowLayout {
                            spacing: 12
                            Layout.fillWidth: true
                            Avatar { name: Backend.hostIdentity.name ?? ""; seed: Backend.hostIdentity.id ?? ""; size: 36 }
                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                TfText { text: Backend.hostIdentity.name ?? ""; font.weight: Font.ExtraBold }
                                TfText { text: Backend.hostIdentity.id ?? ""; variant: "muted" }
                            }
                            Chip { text: Backend.hostIdentity.label ?? ""; tone: "accent" }
                        }
                        TfText { text: qsTr("Open project"); variant: "label"; Layout.topMargin: 4 }
                        TfComboBox {
                            id: hostProject
                            Layout.fillWidth: true
                            model: Backend.requirements
                            textRole: "name"
                            valueRole: "id"
                            Accessible.name: qsTr("Project to open")
                            Component.onCompleted: currentIndex = Math.max(0, indexOfValue(Backend.currentRequirementId))
                        }
                        TfButton {
                            text: qsTr("Preview host")
                            variant: "primary"
                            Layout.topMargin: 4
                            onClicked: Backend.previewWorkspace("host", hostProject.currentValue)
                        }
                    }
                }
            }

            // Local data: what most maintenance needs.
            Section {
                title: qsTr("Local data")
                subtitle: page.status.dirty ? qsTr("Unsaved changes in memory") : qsTr("Everything is saved")
                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 8
                    Field { label: qsTr("Folder"); value: page.status.dataDirectory ?? "" }
                    Flow {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        spacing: 8
                        TfButton {
                            text: qsTr("Save now")
                            variant: "primary"
                            compact: true
                            enabled: Backend.dirty
                            onClicked: if (Backend.save()) page.notify(qsTr("Saved to disk"))
                        }
                        TfButton {
                            text: qsTr("Reload data")
                            compact: true
                            onClicked: Backend.dirty
                                       ? confirmReload.ask(qsTr("Reload from disk?"), qsTr("Unsaved changes in memory are discarded."), qsTr("Reload"))
                                       : (Backend.reload() && page.notify(qsTr("Reloaded from disk")))
                        }
                        TfButton {
                            text: qsTr("Reset seed data")
                            variant: "danger"
                            compact: true
                            onClicked: confirmReset.ask(qsTr("Reset to the seed data?"),
                                                        qsTr("Participants, projects, skills, interest requests and saved teams are replaced with the bundled seed data. The current files are backed up first."),
                                                        qsTr("Reset data"))
                        }
                    }
                    CheckResult { result: page.dataResult; Layout.fillWidth: true }
                }
            }

            // System tools: folded away until needed.
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                spacing: 10
                TfText { text: qsTr("System tools"); variant: "section"; Layout.fillWidth: true }
                TfButton {
                    text: page.showTools ? qsTr("Hide system tools") : qsTr("Show system tools")
                    compact: true
                    onClicked: page.showTools = !page.showTools
                }
            }
            TfText {
                visible: !page.showTools
                text: qsTr("Validation, data structures, persistence and matching checks, the latest error and the activity log.")
                variant: "muted"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            GridLayout {
                visible: page.showTools
                Layout.fillWidth: true
                columns: page.twoColumns ? 2 : 1
                columnSpacing: Theme.gap
                rowSpacing: Theme.gap

                Section {
                    title: qsTr("Data checks")
                    subtitle: qsTr("Local JSON files on this device")
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 8
                        Field {
                            label: qsTr("State")
                            value: page.status.dirty ? qsTr("Unsaved changes in memory") : qsTr("Saved")
                            valueColor: page.status.dirty ? Theme.warning : Theme.success
                        }
                        Field { label: qsTr("Loaded"); value: page.status.lastLoaded ?? "" }
                        Field { label: qsTr("Seed data"); value: page.status.seed || qsTr("Explicit data folder") }
                        Flow {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            spacing: 8
                            TfButton {
                                text: qsTr("Validate data")
                                compact: true
                                onClicked: page.validateResult = Backend.validateData()
                            }
                        }
                        CheckResult { result: page.validateResult; Layout.fillWidth: true }
                    }
                }

                Section {
                    title: qsTr("Data structures")
                    subtitle: page.index.indexSkills === page.index.treeSkills && page.index.graphSkills === page.index.treeSkills
                              ? qsTr("All three structures agree") : qsTr("The structures disagree: rebuild them")
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 8
                        Field { label: qsTr("SkillIndex"); value: qsTr("%1 skills · hash map skill → ids").arg(page.index.indexSkills ?? 0) }
                        Field {
                            label: qsTr("SkillBST")
                            value: qsTr("%1 skills · AVL height %2 · %3").arg(page.index.treeSkills ?? 0).arg(page.index.treeHeight ?? 0)
                                   .arg(page.index.treeBalanced ? qsTr("balanced") : qsTr("UNBALANCED"))
                        }
                        Field { label: qsTr("SkillGraph"); value: qsTr("%1 skills · %2 co-occurrence edges").arg(page.index.graphSkills ?? 0).arg(page.index.graphEdges ?? 0) }
                        Flow {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            spacing: 8
                            TfButton { text: qsTr("Rebuild SkillIndex"); compact: true; onClicked: page.indexResult = Backend.rebuildSkillIndex() }
                            TfButton { text: qsTr("Rebuild SkillBST"); compact: true; onClicked: page.indexResult = Backend.rebuildSkillTree() }
                            TfButton { text: qsTr("Refresh SkillGraph"); compact: true; onClicked: page.indexResult = Backend.rebuildSkillGraph() }
                            TfButton { text: qsTr("Check consistency"); compact: true; onClicked: page.indexResult = Backend.runIndexCheck() }
                        }
                        CheckResult { result: page.indexResult; Layout.fillWidth: true }
                    }
                }

                Section {
                    title: qsTr("Persistence")
                    subtitle: qsTr("Saves write a temporary file, then rename it")
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 8
                        Field { label: qsTr("Files"); value: "students.json · requirements.json · teams.json · skills.json · interest_requests.json" }
                        Field { label: qsTr("Session"); value: page.status.sessionFile ?? "" }
                        Field { label: qsTr("Log"); value: page.status.logFile ?? "" }
                        Field { label: qsTr("Last saved"); value: page.status.lastSaved || qsTr("Not this session") }
                        Flow {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            spacing: 8
                            TfButton {
                                text: qsTr("Run persistence check")
                                compact: true
                                onClicked: page.persistenceResult = Backend.runPersistenceCheck()
                            }
                        }
                        CheckResult { result: page.persistenceResult; Layout.fillWidth: true }
                    }
                }

                Section {
                    title: qsTr("Matching")
                    subtitle: qsTr("Report weights; engagement replaced availability")
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 8
                        RowLayout {
                            spacing: 12
                            TfText { text: qsTr("Strategy"); variant: "muted"; font.weight: Font.Bold; Layout.preferredWidth: 120 }
                            Segmented {
                                options: [{ text: qsTr("Weighted"), value: "weighted" }, { text: qsTr("Coverage baseline"), value: "baseline" }]
                                currentValue: Backend.strategy
                                onActivated: value => Backend.strategy = value
                            }
                        }
                        Repeater {
                            model: Backend.factorDefinitions
                            delegate: RowLayout {
                                id: weight
                                required property var modelData
                                required property int index
                                spacing: 12
                                Layout.fillWidth: true
                                TfText { text: weight.modelData.shortLabel; font.weight: Font.Bold; Layout.preferredWidth: 120 }
                                MeterBar {
                                    Layout.fillWidth: true
                                    value: weight.modelData.weight / 0.4
                                    fillColor: Theme.factorColors[weight.index]
                                }
                                TfText {
                                    text: Theme.percent(weight.modelData.weight)
                                    font.weight: Font.ExtraBold
                                    color: Theme.factorColors[weight.index]
                                    horizontalAlignment: Text.AlignRight
                                    Layout.preferredWidth: 44
                                }
                            }
                        }
                        Flow {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            spacing: 8
                            TfButton {
                                text: qsTr("Run matching smoke test")
                                compact: true
                                onClicked: page.matchingResult = Backend.runMatchingSmokeTest()
                            }
                        }
                        CheckResult { result: page.matchingResult; Layout.fillWidth: true }
                    }
                }
            }

            Section {
                visible: page.showTools
                title: qsTr("Latest error")
                subtitle: page.error.type !== undefined ? qsTr("The latest error in full, from any workspace") : qsTr("No errors this session")
                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    visible: page.error.type !== undefined
                    spacing: 6
                    Field { label: qsTr("Exception"); value: page.error.type ?? ""; valueColor: Theme.accentHover }
                    Field { label: qsTr("Operation"); value: page.error.operation ?? "" }
                    Field { label: qsTr("Message"); value: page.error.message ?? "" }
                    Field { label: qsTr("Record"); value: page.error.record || qsTr("none") }
                    Field { label: qsTr("Workspace"); value: page.error.workspace ?? "" }
                    Field { label: qsTr("Storage"); value: page.error.storage ?? "" }
                    Field { label: qsTr("Time"); value: page.error.time ?? "" }
                }
            }

            Section {
                visible: page.showTools
                title: qsTr("Activity")
                subtitle: qsTr("%1 entries · newest first").arg(page.logEntries.length)
                padded: false
                actions: [
                    Segmented {
                        options: [{ text: qsTr("All"), value: "" }, { text: qsTr("Info"), value: "info" },
                                  { text: qsTr("Warnings"), value: "warning" }, { text: qsTr("Errors"), value: "error" }]
                        currentValue: page.logLevel
                        onActivated: value => page.logLevel = value
                    },
                    TfTextField {
                        placeholderText: qsTr("Filter log")
                        Layout.preferredWidth: 220
                        onTextChanged: page.logQuery = text.trim().toLowerCase()
                    }
                ]
                ListView {
                    implicitHeight: 360
                    anchors.left: parent.left
                    anchors.right: parent.right
                    clip: true
                    model: page.logEntries
                    reuseItems: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        id: logRow
                        required property var modelData
                        width: ListView.view.width
                        implicitHeight: Math.max(40, message.implicitHeight + 16)
                        color: "transparent"
                        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.pad
                            anchors.rightMargin: Theme.pad
                            spacing: 12
                            TfText { text: logRow.modelData.time; variant: "muted"; font.weight: Font.Bold; Layout.preferredWidth: 72 }
                            Chip {
                                text: logRow.modelData.level
                                tone: logRow.modelData.level === "error" ? "accent" : logRow.modelData.level === "warning" ? "warning" : "neutral"
                                Layout.preferredWidth: 80
                            }
                            TfText {
                                text: logRow.modelData.category
                                font.weight: Font.ExtraBold
                                color: Theme.textSecondary
                                Layout.preferredWidth: 96
                            }
                            TfText {
                                id: message
                                text: logRow.modelData.message
                                wrapMode: Text.WrapAnywhere
                                elide: Text.ElideNone
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 16 }
        }
    }
}

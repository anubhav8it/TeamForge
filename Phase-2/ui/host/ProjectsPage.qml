pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Project list + editor. The draft below is form state only; saving hands the whole project to
// Backend.saveRequirement, where the core validates it. The developer workspace reuses the page
// (developerMode) with raw-record inspection for projects and saved teams.
TfPage {
    id: page

    property bool developerMode: false
    signal openMatching()
    signal notify(string message)
    signal inspect(string kind, string id)
    signal explainFit(string studentId, string requirementId)

    // Editor state
    property string editingId: ""   // "" while creating a new project
    property bool creating: false
    property string draftName: ""
    property string draftType: ""
    property string draftSummary: ""
    property int draftMin: 2
    property int draftMax: 4
    property bool modified: false
    property bool revealNewSkill: false

    readonly property var projectTypes: [qsTr("Hackathon"), qsTr("PBL / Course Project"), qsTr("AI/ML Project"),
                                         qsTr("Web / Product Project"), qsTr("Research Project"),
                                         qsTr("Technology Competition")]
    // A type outside the presets (from older data) still shows, as an extra pill.
    readonly property var typeOptions: draftType !== "" && projectTypes.indexOf(draftType) < 0
                                       ? projectTypes.concat([draftType]) : projectTypes

    ListModel { id: draftSkills }

    // Developer: the project's interest requests, with accept / decline.
    Drawer {
        id: interestDrawer
        parent: Overlay.overlay
        edge: Qt.RightEdge
        readonly property real panelWidth: Math.min(980, Math.max(700, (parent?.width ?? 1200) * 0.7))
        width: panelWidth
        height: parent.height
        contentWidth: panelWidth - 2 * padding
        modal: true
        dim: true
        padding: 24
        Overlay.modal: Rectangle { color: "#8C0B0D11" }
        background: Rectangle { color: Theme.surface; Rectangle { width: 1; height: parent.height; color: Theme.borderStrong } }
        contentItem: ColumnLayout {
            spacing: 14
            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true
                    TfText { text: qsTr("Applicants"); variant: "eyebrow"; color: Theme.violet }
                    TfText { text: page.draftName; font.pixelSize: 26; font.weight: Font.ExtraBold }
                }
                TfButton { text: "✕"; variant: "ghost"; compact: true; Accessible.name: qsTr("Close applicants"); onClicked: interestDrawer.close() }
            }
            InterestReviewList {
                Layout.fillWidth: true
                Layout.fillHeight: true
                requirementId: interestDrawer.opened ? page.editingId : ""
                onWhyThisMatch: (studentId, requirementId) => page.explainFit(studentId, requirementId)
                onNotify: message => page.notify(message)
            }
        }
    }

    function projectById(id) {
        return Backend.requirements.find(p => p.id === id)
    }

    // Shows `project` in the editor, or clears the editor when it is undefined.
    function load(project) {
        creating = false
        editingId = project ? project.id : ""
        draftName = project ? project.name : ""
        draftType = project ? project.type : ""
        draftSummary = project ? (project.summary ?? "") : ""
        draftMin = project ? project.minTeamSize : 2
        draftMax = project ? project.maxTeamSize : 4
        draftSkills.clear()
        if (project) {
            for (const skill of project.requiredSkills)
                draftSkills.append({ skill: skill.skill, minLevel: skill.minLevel })
        }
        modified = false
    }

    function loadActiveOrNothing() {
        load(Backend.currentRequirement.id !== undefined ? Backend.currentRequirement : undefined)
    }

    function startNew() {
        load(undefined)
        creating = true
        draftSkills.append({ skill: "", minLevel: 3 })
        nameField.forceActiveFocus()
    }

    function save() {
        const skills = []
        for (let i = 0; i < draftSkills.count; ++i) {
            const row = draftSkills.get(i)
            if (row.skill.trim() !== "")
                skills.push({ skill: row.skill, minLevel: row.minLevel })
        }
        const id = creating ? Backend.newRequirementId() : editingId
        // Saved to disk straight away, like a saved team.
        const ok = Backend.saveRequirement({
            id: id, name: draftName, type: draftType, summary: draftSummary, requiredSkills: skills,
            minTeamSize: draftMin, maxTeamSize: draftMax
        }) && Backend.save()
        if (!ok)
            return
        const created = creating
        if (created)
            Backend.currentRequirementId = id
        load(projectById(id))
        notify(created ? qsTr("Project created and set active") : qsTr("Project updated"))
    }


    // Open the active project when the page is first shown or the data reloads.
    onActiveChanged: if (active && editingId === "" && !creating) loadActiveOrNothing()
    Connections {
        target: Backend
        function onDataChanged() {
            if (!page.creating && page.editingId !== "" && !page.modified) {
                const fresh = page.projectById(page.editingId)
                if (fresh) page.load(fresh)
            }
        }
    }

    component SectionTitle: RowLayout {
        id: section
        property int number
        property string text
        property string hint
        spacing: 10
        Layout.fillWidth: true
        Rectangle {
            implicitWidth: 24
            implicitHeight: 24
            radius: 12
            color: Theme.accentSubtle
            Text {
                anchors.centerIn: parent
                text: section.number
                font.family: Theme.fontFamily
                font.pixelSize: 12
                font.weight: Font.ExtraBold
                color: Theme.accentHover
            }
        }
        TfText {
            text: section.text
            variant: "heading"
            font.pixelSize: Theme.fontLg
        }
        TfText {
            text: section.hint
            variant: "muted"
            Layout.fillWidth: true
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 24
        anchors.topMargin: 20
        spacing: Theme.gap

        ColumnLayout {
            spacing: Theme.gap
            Layout.preferredWidth: 320
            Layout.minimumWidth: 280
            Layout.fillWidth: false
            Layout.fillHeight: true

        // Project list
        Panel {
            title: qsTr("Projects")
            subtitle: Theme.count(Backend.requirements.length, qsTr("project"), qsTr("projects"))
            padded: false
            Layout.fillWidth: true
            Layout.fillHeight: true

            actions: TfButton {
                text: qsTr("+ New")
                variant: "primary"
                compact: true
                onClicked: page.startNew()
            }

            ListView {
                anchors.fill: parent
                clip: true
                model: Backend.requirements
                boundsBehavior: Flickable.StopAtBounds
                ScrollIndicator.vertical: ScrollIndicator {}
                delegate: Rectangle {
                    id: row
                    required property var modelData
                    readonly property bool selected: !page.creating && page.editingId === modelData.id
                    readonly property bool isActive: modelData.id === Backend.currentRequirementId
                    Accessible.role: Accessible.ListItem
                    Accessible.name: modelData.name
                    Accessible.onPressAction: page.load(row.modelData)
                    width: ListView.view.width
                    implicitHeight: 68
                    color: selected ? Theme.selection : rowHover.hovered ? Theme.surfaceRaised : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.animFast } }
                    Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }
                    Rectangle {
                        visible: row.selected
                        width: 4
                        height: parent.height
                        color: Theme.accent
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.pad
                        anchors.rightMargin: Theme.pad
                        spacing: 12
                        Rectangle {
                            implicitWidth: 10
                            implicitHeight: 10
                            radius: 5
                            color: Theme.typeColor(row.modelData.type ?? "")
                        }
                        ColumnLayout {
                            spacing: 2
                            Layout.fillWidth: true
                            RowLayout {
                                Layout.fillWidth: true
                                TfText {
                                    text: row.modelData.name
                                    font.weight: Font.ExtraBold
                                    Layout.fillWidth: true
                                }
                                Chip {
                                    visible: row.isActive
                                    text: qsTr("Active")
                                    tone: "accent"
                                }
                            }
                            TfText {
                                text: [page.developerMode ? row.modelData.id : row.modelData.type,
                                       qsTr("%1–%2 members").arg(row.modelData.minTeamSize).arg(row.modelData.maxTeamSize)]
                                      .filter(part => part).join(" · ")
                                variant: "muted"
                                font.pixelSize: Theme.fontXs
                                Layout.fillWidth: true
                            }
                        }
                    }
                    HoverHandler {
                        id: rowHover
                        cursorShape: Qt.PointingHandCursor
                    }
                    TapHandler {
                        onTapped: page.load(row.modelData)
                    }
                }
            }
        }

        // Developer: saved teams as records.
        Panel {
            visible: page.developerMode
            title: qsTr("Saved teams")
            subtitle: Theme.count(Backend.savedTeams.length, qsTr("team"), qsTr("teams"))
            padded: false
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(260, 76 + Backend.savedTeams.length * 52)
            ListView {
                anchors.fill: parent
                clip: true
                model: Backend.savedTeams
                boundsBehavior: Flickable.StopAtBounds
                delegate: Rectangle {
                    id: teamRow
                    required property var modelData
                    width: ListView.view.width
                    implicitHeight: 52
                    color: teamHover.hovered ? Theme.surfaceRaised : "transparent"
                    Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.pad
                        anchors.rightMargin: Theme.pad
                        spacing: 0
                        TfText { text: teamRow.modelData.id; font.weight: Font.ExtraBold; Layout.fillWidth: true }
                        TfText {
                            text: qsTr("%1 · %2 members · %3 coverage").arg(teamRow.modelData.requirementName ?? "")
                                  .arg(teamRow.modelData.size ?? 0).arg(Theme.percent(teamRow.modelData.coverageRatio ?? 0))
                            variant: "muted"
                            font.pixelSize: Theme.fontXs
                            Layout.fillWidth: true
                        }
                    }
                    HoverHandler { id: teamHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: page.inspect("team", teamRow.modelData.id) }
                }
                EmptyState {
                    anchors.centerIn: parent
                    width: 260
                    visible: Backend.savedTeams.length === 0
                    title: qsTr("No saved teams yet")
                }
            }
        }
        }

        // Editor
        Panel {
            title: page.creating ? qsTr("New project") : (page.draftName || qsTr("Edit project"))
            subtitle: page.creating ? qsTr("Matching uses it as soon as you create it")
                                    : page.modified ? qsTr("Unsaved edits") : qsTr("Changes apply to matching when saved")
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: page.creating || page.editingId !== ""

            actions: [
                TfButton {
                    visible: !page.creating && page.editingId !== Backend.currentRequirementId
                    text: qsTr("Set active")
                    variant: "secondary"
                    compact: true
                    onClicked: Backend.currentRequirementId = page.editingId
                },
                TfButton {
                    visible: page.developerMode && !page.creating
                    text: qsTr("Applicants %1").arg(page.projectById(page.editingId)?.interest?.total ?? 0)
                    variant: "secondary"
                    compact: true
                    onClicked: interestDrawer.open()
                },
                TfButton {
                    visible: page.developerMode && !page.creating
                    text: qsTr("Raw record")
                    variant: "secondary"
                    compact: true
                    onClicked: page.inspect("project", page.editingId)
                },
                TfButton {
                    visible: !page.developerMode && !page.creating && page.editingId === Backend.currentRequirementId
                    text: qsTr("Find teammates")
                    variant: "secondary"
                    compact: true
                    onClicked: page.openMatching()
                }
            ]

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                ScrollView {
                    id: editorScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth

                    // Bring a newly added skill row (and the Add button) into view.
                    Connections {
                        target: editorScroll.contentItem
                        function onContentHeightChanged() {
                            if (!page.revealNewSkill)
                                return
                            const flick = editorScroll.contentItem as Flickable
                            flick.contentY = Math.max(0, flick.contentHeight - flick.height)
                            page.revealNewSkill = false
                        }
                    }
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: 26

                        // 1. Basics
                        ColumnLayout {
                            spacing: 12
                            Layout.fillWidth: true
                            SectionTitle { number: 1; text: qsTr("Basics") }
                            TfTextField {
                                id: nameField
                                text: page.draftName
                                placeholderText: qsTr("Project name, e.g. Smart Campus Hackathon")
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.Bold
                                Layout.fillWidth: true
                                Layout.maximumWidth: 560
                                // textChanged (not textEdited) so values set by assistive technology are captured too.
                                onTextChanged: if (text !== page.draftName) { page.draftName = text; page.modified = true }
                            }
                            TfTextField {
                                text: page.draftSummary
                                placeholderText: qsTr("One-line description (optional), e.g. Campus Energy Intelligence")
                                Layout.fillWidth: true
                                Layout.maximumWidth: 560
                                onTextChanged: if (text !== page.draftSummary) { page.draftSummary = text; page.modified = true }
                            }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 8
                                Repeater {
                                    model: page.typeOptions
                                    delegate: Rectangle {
                                        id: preset
                                        required property string modelData
                                        readonly property bool chosen: page.draftType === modelData
                                        readonly property color tone: Theme.typeColor(modelData)
                                        Accessible.role: Accessible.RadioButton
                                        Accessible.name: modelData
                                        Accessible.checked: chosen
                                        Accessible.onPressAction: { page.draftType = preset.modelData; page.modified = true }
                                        implicitWidth: presetLabel.implicitWidth + 40
                                        implicitHeight: 36
                                        radius: 18
                                        color: chosen ? Qt.rgba(tone.r, tone.g, tone.b, 0.18)
                                                      : presetHover.hovered ? Theme.surfaceRaised : "transparent"
                                        border.width: chosen ? 2 : 1
                                        border.color: chosen ? tone : presetHover.hovered ? Theme.borderStrong : Theme.border
                                        Behavior on color { ColorAnimation { duration: Theme.animFast } }
                                        Rectangle {
                                            x: 14
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 8
                                            height: 8
                                            radius: 4
                                            color: preset.tone
                                        }
                                        Text {
                                            id: presetLabel
                                            x: 28
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: preset.modelData
                                            textFormat: Text.PlainText
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSm
                                            font.weight: Font.Bold
                                            color: preset.chosen ? Theme.text : Theme.textSecondary
                                        }
                                        HoverHandler { id: presetHover; cursorShape: Qt.PointingHandCursor }
                                        TapHandler {
                                            onTapped: { page.draftType = preset.modelData; page.modified = true }
                                        }
                                    }
                                }
                            }
                        }

                        // 2. Team size
                        ColumnLayout {
                            spacing: 12
                            Layout.fillWidth: true
                            SectionTitle { number: 2; text: qsTr("Team size"); hint: qsTr("Smallest and largest team allowed") }
                            RowLayout {
                                spacing: 10
                                TfSpinBox {
                                    from: 1
                                    to: 20
                                    value: page.draftMin
                                    Accessible.name: qsTr("Minimum team size")
                                    // valueChanged (guarded) so values set by assistive technology are captured too.
                                    onValueChanged: if (value !== page.draftMin) { page.draftMin = value; page.modified = true }
                                }
                                TfText { text: qsTr("to"); variant: "secondary"; font.weight: Font.Bold }
                                TfSpinBox {
                                    from: 1
                                    to: 20
                                    value: page.draftMax
                                    Accessible.name: qsTr("Maximum team size")
                                    onValueChanged: if (value !== page.draftMax) { page.draftMax = value; page.modified = true }
                                }
                                TfText { text: qsTr("members"); variant: "secondary"; font.weight: Font.Bold }
                            }
                        }

                        // 3. Required skills
                        ColumnLayout {
                            spacing: 8
                            Layout.fillWidth: true
                            SectionTitle { number: 3; text: qsTr("Required skills"); hint: qsTr("And the minimum level for each (1–5)") }
                            Repeater {
                                model: draftSkills
                                delegate: RowLayout {
                                    id: skillRow
                                    required property int index
                                    required property string skill
                                    required property int minLevel
                                    Layout.fillWidth: true
                                    Layout.maximumWidth: 640
                                    spacing: 12
                                    SkillField {
                                        text: skillRow.skill
                                        Layout.fillWidth: true
                                        onTextChanged: if (text !== skillRow.skill) {
                                            draftSkills.setProperty(skillRow.index, "skill", text)
                                            page.modified = true
                                        }
                                    }
                                    LevelPips {
                                        interactive: true
                                        level: skillRow.minLevel
                                        onPicked: newLevel => {
                                            draftSkills.setProperty(skillRow.index, "minLevel", newLevel)
                                            page.modified = true
                                        }
                                    }
                                    TfText {
                                        text: qsTr("Level %1+").arg(skillRow.minLevel)
                                        variant: "secondary"
                                        font.weight: Font.Bold
                                        Layout.preferredWidth: 64
                                    }
                                    TfButton {
                                        text: "✕"
                                        variant: "ghost"
                                        compact: true
                                        Layout.preferredWidth: 34
                                        Accessible.name: qsTr("Remove skill %1").arg(skillRow.skill)
                                        onClicked: { draftSkills.remove(skillRow.index); page.modified = true }
                                    }
                                }
                            }
                            TfButton {
                                text: qsTr("+ Add skill")
                                variant: "secondary"
                                compact: true
                                Layout.topMargin: 4
                                onClicked: {
                                    draftSkills.append({ skill: "", minLevel: 3 })
                                    page.modified = true
                                    page.revealNewSkill = true // scrolled once the content has grown
                                }
                            }
                        }

                    }
                }

                // Footer actions
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border; Layout.bottomMargin: 14 }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    TfButton {
                        visible: !page.creating
                        text: qsTr("Delete project")
                        variant: "danger"
                        compact: true
                        onClicked: confirmDelete.ask(qsTr("Delete %1?").arg(page.draftName),
                                                     qsTr("The project and its requirements are removed from TeamForge. This can't be undone."),
                                                     qsTr("Delete project"))
                        ConfirmDialog {
                            id: confirmDelete
                            onConfirmed: {
                                if (Backend.removeRequirement(page.editingId) && Backend.save()) {
                                    page.notify(qsTr("Project deleted"))
                                    page.loadActiveOrNothing()
                                }
                            }
                        }
                    }
                    Item { Layout.fillWidth: true }
                    TfButton {
                        text: page.creating ? qsTr("Cancel") : qsTr("Revert")
                        variant: "ghost"
                        enabled: page.modified || page.creating
                        onClicked: page.creating ? page.loadActiveOrNothing() : page.load(page.projectById(page.editingId))
                    }
                    TfButton {
                        text: page.creating ? qsTr("Create project") : qsTr("Save project")
                        variant: "primary"
                        enabled: page.modified || page.creating
                        onClicked: page.save()
                    }
                }
            }
        }

        // Nothing selected and no projects
        Panel {
            visible: !page.creating && page.editingId === ""
            Layout.fillWidth: true
            Layout.fillHeight: true
            EmptyState {
                anchors.centerIn: parent
                width: 340
                title: qsTr("No project selected")
                message: qsTr("Pick a project on the left or create a new one.")
                actionText: qsTr("New project")
                onActionTriggered: page.startNew()
            }
        }
    }
}

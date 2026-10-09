pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// One record from the developer databases (Backend.inspectRecord).
//
// Participants open on a structured inspector: Profile (name, academics, summary, experience),
// Skills (levels), Learning interests and Matching data, all editable and saved through
// Backend.saveStudent. Raw data shows the exact stored form (editable JSON, validated like the
// data files by Backend.applyRecordJson) and compares it with the copy on disk and with the
// skill structures. Projects, saved teams and skills open on the raw views.
Drawer {
    id: inspector

    signal notify(string message)

    property string kind
    property string recordId
    property var record: ({})
    property string tab: "profile"
    property bool edited: false      // raw JSON edited
    property bool modified: false    // structured fields edited
    property string applyError: ""

    // Structured draft (participants).
    property string draftName
    property string draftProgram
    property string draftSummary
    property string draftRole
    property var draftWanted: []
    ListModel { id: draftSkills }

    readonly property bool structured: kind === "participant"
    readonly property var tabs: structured
        ? [{ text: qsTr("Profile"), value: "profile" }, { text: qsTr("Skills"), value: "skills" },
           { text: qsTr("Learning"), value: "learning" }, { text: qsTr("Matching data"), value: "matching" },
           { text: qsTr("Raw data"), value: "raw" }]
        : [{ text: qsTr("Record"), value: "raw" }, { text: qsTr("On disk"), value: "stored" },
           { text: qsTr("Indexes"), value: "indexes" }, { text: qsTr("Matching"), value: "matching" }]
    readonly property var pageIndex: ({ profile: 0, skills: 1, learning: 2, raw: 3, stored: 4, indexes: 5, matching: 6 })

    readonly property var kindNames: ({ participant: qsTr("Participant"), project: qsTr("Project"),
                                        team: qsTr("Saved team"), skill: qsTr("Skill") })
    readonly property var stateInfo: ({
        same: { text: qsTr("Saved"), tone: "success" },
        different: { text: qsTr("Unsaved changes"), tone: "warning" },
        missing: { text: qsTr("Not saved yet"), tone: "warning" },
        error: { text: qsTr("Stored data unreadable"), tone: "accent" }
    })

    function show(newKind, id) {
        kind = newKind
        recordId = id
        tab = newKind === "participant" ? "profile" : "raw"
        refresh()
        open()
    }
    function refresh() {
        record = Backend.inspectRecord(kind, recordId)
        rawEditor.text = record.loaded ?? ""
        edited = false
        modified = false
        applyError = ""
        if (structured)
            loadDraft()
    }
    // The structured fields come from the record as the app holds it.
    function loadDraft() {
        let data = {}
        try {
            data = JSON.parse(record.loaded ?? "{}")
        } catch (e) {
            data = {}
        }
        draftName = data.name ?? ""
        draftProgram = data.program ?? ""
        draftSummary = data.summary ?? ""
        draftRole = data.role ?? ""
        draftWanted = (data.skillsWanted ?? []).slice()
        draftSkills.clear()
        const offered = data.skillsOffered ?? {}
        const names = Object.keys(offered).sort((a, b) => offered[b] - offered[a])
        for (const name of names)
            draftSkills.append({ skill: name, level: offered[name] })
        modified = false
    }
    function draft() {
        const skills = []
        for (let i = 0; i < draftSkills.count; ++i)
            skills.push({ skill: draftSkills.get(i).skill, level: draftSkills.get(i).level })
        return { id: recordId, name: draftName, program: draftProgram, summary: draftSummary, role: draftRole,
                 skillsOffered: skills, skillsWanted: draftWanted }
    }
    function saveDraft(toDisk) {
        if (!Backend.saveStudent(draft())) {
            applyError = Backend.lastErrorDetail.message ?? Backend.lastError
            return
        }
        if (toDisk && !Backend.save()) {
            applyError = Backend.lastError
            return
        }
        refresh()
        notify(toDisk ? qsTr("Participant saved") : qsTr("Participant updated; save to write it to disk"))
    }
    function applyRaw(andSave) {
        // The record may have been given a new id (or a participant/project added by copying).
        let nextId = recordId
        try {
            const parsed = JSON.parse(rawEditor.text)
            nextId = kind === "skill" ? recordId : (parsed.id ?? recordId)
        } catch (e) {
            // invalid JSON: the backend reports it
        }
        if (!Backend.applyRecordJson(kind, rawEditor.text)) {
            applyError = Backend.lastErrorDetail.message ?? Backend.lastError
            return
        }
        if (andSave && !Backend.save()) {
            applyError = Backend.lastError
            return
        }
        recordId = nextId
        refresh()
        notify(andSave ? qsTr("Record saved to disk") : qsTr("Record applied; save to write it to disk"))
    }

    Connections {
        target: Backend
        function onDataChanged() { if (inspector.opened && !inspector.edited && !inspector.modified) inspector.refresh() }
        function onDirtyChanged() { if (inspector.opened && !inspector.edited && !inspector.modified) inspector.refresh() }
    }

    parent: Overlay.overlay
    edge: Qt.RightEdge
    readonly property real panelWidth: Math.min(880, Math.max(620, (parent?.width ?? 1200) * 0.6))
    width: panelWidth
    height: parent.height
    contentWidth: panelWidth
    modal: true
    dim: true
    padding: 0

    Overlay.modal: Rectangle { color: "#8C0B0D11" }
    background: Rectangle {
        color: Theme.surface
        Rectangle { width: 1; height: parent.height; color: Theme.borderStrong }
    }

    // Labelled field row for the structured view.
    component FieldLabel: TfText {
        variant: "label"
        Layout.topMargin: 6
    }

    contentItem: ColumnLayout {
        spacing: 16

        // Header
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 24
            Layout.bottomMargin: 0
            spacing: 16
            Avatar {
                visible: inspector.structured
                name: inspector.draftName
                seed: inspector.recordId
                size: 64
            }
            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true
                TfText {
                    text: inspector.kindNames[inspector.kind] ?? ""
                    variant: "eyebrow"
                    color: Theme.violet
                }
                TfText {
                    text: inspector.kind === "skill" ? Theme.skill(inspector.record.title ?? "") : (inspector.record.title ?? "")
                    font.pixelSize: 28
                    font.weight: Font.ExtraBold
                    Layout.fillWidth: true
                }
                RowLayout {
                    spacing: 8
                    TfText {
                        text: inspector.record.subtitle ?? ""
                        variant: "muted"
                        font.family: inspector.structured ? Theme.monoFamily : Theme.fontFamily
                        font.weight: Font.Bold
                    }
                    Chip {
                        visible: inspector.record.storedState !== undefined
                        text: inspector.stateInfo[inspector.record.storedState ?? "same"].text
                        tone: inspector.stateInfo[inspector.record.storedState ?? "same"].tone
                    }
                    Chip {
                        visible: inspector.structured && Backend.workspace === "developer"
                        text: qsTr("Preview as this participant")
                        tone: "tint"
                        tint: Theme.info
                        Accessible.role: Accessible.Button
                        Accessible.name: text
                        Accessible.onPressAction: { inspector.close(); Backend.previewWorkspace("participant", inspector.recordId) }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: { inspector.close(); Backend.previewWorkspace("participant", inspector.recordId) } }
                    }
                }
            }
            TfButton {
                text: "✕"
                variant: "ghost"
                compact: true
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: 36
                Accessible.name: qsTr("Close record")
                onClicked: inspector.close()
            }
        }

        TfText {
            visible: (inspector.record.error ?? "") !== ""
            text: inspector.record.error ?? ""
            color: Theme.accentHover
            font.weight: Font.Bold
            Layout.leftMargin: 24
        }

        Segmented {
            Layout.leftMargin: 24
            options: inspector.tabs
            currentValue: inspector.tab
            onActivated: value => inspector.tab = value
        }

        StackLayout {
            currentIndex: inspector.pageIndex[inspector.tab] ?? 3
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            // 0 Profile: name, academics, summary, experience.
            ColumnLayout {
                spacing: 8
                FieldLabel { text: qsTr("Name") }
                TfTextField {
                    text: inspector.draftName
                    placeholderText: qsTr("Full name")
                    font.pixelSize: Theme.fontMd
                    font.weight: Font.Bold
                    Layout.fillWidth: true
                    onTextChanged: if (text !== inspector.draftName) { inspector.draftName = text; inspector.modified = true }
                }
                FieldLabel { text: qsTr("Academic details") }
                TfTextField {
                    text: inspector.draftProgram
                    placeholderText: qsTr("Programme and year")
                    Layout.fillWidth: true
                    onTextChanged: if (text !== inspector.draftProgram) { inspector.draftProgram = text; inspector.modified = true }
                }
                FieldLabel { text: qsTr("One-line summary") }
                TfTextField {
                    text: inspector.draftSummary
                    placeholderText: qsTr("Skills and experience in one sentence")
                    Layout.fillWidth: true
                    onTextChanged: if (text !== inspector.draftSummary) { inspector.draftSummary = text; inspector.modified = true }
                }
                FieldLabel { text: qsTr("Experience") }
                TfText {
                    readonly property var row: Backend.students.find(s => s.id === inspector.recordId) ?? ({})
                    text: (row.experience ?? "") + "  ·  " + qsTr("average level %1 across %2")
                          .arg((row.averageLevel ?? 0).toFixed(1)).arg(Theme.count(draftSkills.count, qsTr("skill"), qsTr("skills")))
                    font.weight: Font.Bold
                    color: Theme.warning
                }
                Item { Layout.fillHeight: true }
            }

            // 1 Skills and levels.
            ColumnLayout {
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    SkillField {
                        id: newSkill
                        placeholderText: qsTr("Add a skill, e.g. ROS2")
                        Layout.fillWidth: true
                    }
                    TfButton {
                        text: qsTr("+ Add")
                        enabled: newSkill.text.trim() !== ""
                        onClicked: {
                            draftSkills.append({ skill: newSkill.text.trim(), level: 3 })
                            newSkill.clear()
                            inspector.modified = true
                        }
                    }
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: draftSkills
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        id: skillRow
                        required property int index
                        required property string skill
                        required property int level
                        width: ListView.view.width
                        height: 48
                        color: "transparent"
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
                        RowLayout {
                            anchors.fill: parent
                            spacing: 14
                            TfText { text: Theme.skill(skillRow.skill); font.weight: Font.ExtraBold; font.letterSpacing: 0.4; Layout.fillWidth: true }
                            LevelPips {
                                interactive: true
                                level: skillRow.level
                                onPicked: newLevel => { draftSkills.setProperty(skillRow.index, "level", newLevel); inspector.modified = true }
                            }
                            TfText { text: Theme.levelName(skillRow.level); variant: "secondary"; font.weight: Font.Bold; Layout.preferredWidth: 124 }
                            TfButton {
                                text: "✕"
                                variant: "ghost"
                                compact: true
                                Layout.preferredWidth: 34
                                Accessible.name: qsTr("Remove skill %1").arg(skillRow.skill)
                                onClicked: { draftSkills.remove(skillRow.index); inspector.modified = true }
                            }
                        }
                    }
                }
            }

            // 2 Learning interests.
            ColumnLayout {
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    SkillField {
                        id: newWanted
                        placeholderText: qsTr("Add a learning interest")
                        Layout.fillWidth: true
                    }
                    TfButton {
                        text: qsTr("+ Add")
                        enabled: newWanted.text.trim() !== ""
                        onClicked: {
                            inspector.draftWanted = inspector.draftWanted.concat([newWanted.text.trim()])
                            newWanted.clear()
                            inspector.modified = true
                        }
                    }
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: inspector.draftWanted
                        delegate: Chip {
                            id: wantedChip
                            required property string modelData
                            skill: true
                            large: true
                            text: modelData + "  ✕"
                            tone: "info"
                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Remove %1").arg(modelData)
                            Accessible.onPressAction: { inspector.draftWanted = inspector.draftWanted.filter(s => s !== wantedChip.modelData); inspector.modified = true }
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: { inspector.draftWanted = inspector.draftWanted.filter(s => s !== wantedChip.modelData); inspector.modified = true } }
                        }
                    }
                }
                TfText {
                    visible: inspector.draftWanted.length === 0
                    text: qsTr("No learning interests")
                    variant: "muted"
                }
                Item { Layout.fillHeight: true }
            }

            // 3 Raw data: the exact stored form, editable.
            ColumnLayout {
                spacing: 10
                TfText {
                    text: inspector.record.editable
                          ? qsTr("The record exactly as stored. Edits are validated like the data files.")
                          : qsTr("The record exactly as stored. Saved teams change through Matching.")
                    variant: "muted"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusSmall
                    color: Theme.input
                    border.color: rawEditor.activeFocus ? Theme.violet : Theme.border
                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 2
                        TextArea {
                            id: rawEditor
                            readOnly: !inspector.record.editable
                            font.family: Theme.monoFamily
                            font.pixelSize: 14
                            color: Theme.text
                            selectionColor: Theme.violet
                            selectedTextColor: "#FFFFFF"
                            selectByMouse: true
                            wrapMode: TextEdit.NoWrap
                            background: null
                            Accessible.name: qsTr("Raw record")
                            onTextChanged: if (activeFocus) inspector.edited = text !== (inspector.record.loaded ?? "")
                        }
                    }
                }
                RowLayout {
                    visible: inspector.record.editable ?? false
                    Layout.fillWidth: true
                    spacing: 8
                    TfText {
                        text: inspector.structured ? qsTr("Compared with disk: %1").arg(inspector.stateInfo[inspector.record.storedState ?? "same"].text)
                                                   : (inspector.edited ? qsTr("Edited") : qsTr("No edits"))
                        variant: "muted"
                        font.weight: Font.Bold
                        Layout.fillWidth: true
                    }
                    TfButton { text: qsTr("Revert"); variant: "ghost"; enabled: inspector.edited; onClicked: inspector.refresh() }
                    TfButton { text: qsTr("Apply"); enabled: inspector.edited; onClicked: inspector.applyRaw(false) }
                    TfButton {
                        text: qsTr("Apply and save")
                        variant: "primary"
                        enabled: inspector.edited || (inspector.record.storedState ?? "same") !== "same"
                        onClicked: inspector.edited ? inspector.applyRaw(true) : (Backend.save() && inspector.notify(qsTr("Saved to disk")))
                    }
                }
            }

            // 4 On disk.
            ColumnLayout {
                spacing: 10
                TfText {
                    text: qsTr("Read from %1 just now.").arg(inspector.record.storagePath ?? "")
                    variant: "muted"
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
                TfText {
                    visible: (inspector.record.storedNote ?? "") !== ""
                    text: inspector.record.storedNote ?? ""
                    color: Theme.accentHover
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusSmall
                    color: Theme.input
                    border.color: Theme.border
                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 2
                        TextArea {
                            readOnly: true
                            text: (inspector.record.stored ?? "") !== "" ? inspector.record.stored : qsTr("(not in the stored file)")
                            font.family: Theme.monoFamily
                            font.pixelSize: 14
                            color: Theme.textSecondary
                            selectByMouse: true
                            wrapMode: TextEdit.NoWrap
                            background: null
                        }
                    }
                }
            }

            // 5 Indexes.
            RecordTable {
                columns: inspector.record.indexColumns ?? []
                rows: inspector.record.indexed ?? []
                note: inspector.kind === "project" ? qsTr("Supply for each required skill: who offers it and who meets the minimum.") : ""
            }

            // 6 Matching (for participants, also the skill structures).
            ColumnLayout {
                spacing: 12
                RecordTable {
                    columns: inspector.record.matchingColumns ?? []
                    rows: inspector.record.matching ?? []
                    note: inspector.kind === "participant" ? qsTr("Fit for each project as the participant's first team: skills met and the weighted score.")
                        : inspector.kind === "project" ? qsTr("Top of the ranking for an empty team, with each factor (0–1).")
                        : ""
                    Layout.fillHeight: true
                }
                RecordTable {
                    visible: inspector.structured
                    columns: inspector.record.indexColumns ?? []
                    rows: inspector.record.indexed ?? []
                    note: qsTr("Advanced: each skill's entries in SkillIndex, SkillBST and SkillGraph.")
                    Layout.preferredHeight: 220
                    Layout.fillHeight: false
                }
            }
        }

        TfText {
            visible: inspector.applyError !== ""
            text: inspector.applyError
            color: Theme.accentHover
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
        }

        // Structured edits: one save for all fields.
        RowLayout {
            visible: inspector.structured && inspector.tab !== "raw" && inspector.tab !== "matching"
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.bottomMargin: 20
            spacing: 8
            TfText {
                text: inspector.modified ? qsTr("Unsaved edits") : qsTr("No edits")
                color: inspector.modified ? Theme.warning : Theme.textMuted
                font.weight: Font.Bold
                Layout.fillWidth: true
            }
            TfButton { text: qsTr("Revert"); variant: "ghost"; enabled: inspector.modified; onClicked: inspector.loadDraft() }
            TfButton {
                text: qsTr("Save participant")
                variant: "primary"
                enabled: inspector.modified
                onClicked: inspector.saveDraft(true)
            }
        }
        Item { visible: !(inspector.structured && inspector.tab !== "raw" && inspector.tab !== "matching"); Layout.preferredHeight: 8 }
    }

    // A small read-only table: label column, value columns, a status dot.
    component RecordTable: ColumnLayout {
        id: table
        property var columns: []
        property var rows: []
        property string note
        spacing: 8
        TfText {
            visible: table.note !== ""
            text: table.note
            variant: "muted"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusSmall
            color: Theme.surfaceRaised
            border.color: Theme.border
            ListView {
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: table.rows
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                header: RowLayout {
                    width: ListView.view.width
                    height: 40
                    spacing: 8
                    Item { Layout.preferredWidth: 14 }
                    TfText { text: ""; Layout.fillWidth: true }
                    Repeater {
                        model: table.columns
                        delegate: TfText {
                            required property string modelData
                            text: modelData
                            variant: "label"
                            horizontalAlignment: Text.AlignRight
                            Layout.preferredWidth: table.columns.length > 4 ? 74 : 96
                        }
                    }
                    Item { Layout.preferredWidth: 10 }
                }
                delegate: Rectangle {
                    id: line
                    required property var modelData
                    required property int index
                    width: ListView.view.width
                    height: 40
                    color: index % 2 === 0 ? "transparent" : Qt.rgba(1, 1, 1, 0.025)
                    RowLayout {
                        anchors.fill: parent
                        spacing: 8
                        Item {
                            Layout.preferredWidth: 14
                            Layout.fillHeight: true
                            Rectangle {
                                anchors.centerIn: parent
                                anchors.horizontalCenterOffset: 4
                                width: 8
                                height: 8
                                radius: 4
                                color: line.modelData.ok ? Theme.success : Theme.warning
                            }
                        }
                        TfText {
                            text: line.modelData.skill ? Theme.skill(line.modelData.label) : line.modelData.label
                            font.weight: Font.Bold
                            font.letterSpacing: line.modelData.skill ? 0.4 : 0
                            Layout.fillWidth: true
                        }
                        Repeater {
                            model: line.modelData.values
                            delegate: TfText {
                                required property string modelData
                                text: modelData
                                font.weight: Font.DemiBold
                                color: modelData === "✗" ? Theme.accentHover : modelData === "✓" ? Theme.success : Theme.text
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: table.columns.length > 4 ? 74 : 96
                            }
                        }
                        Item { Layout.preferredWidth: 10 }
                    }
                }
                EmptyState {
                    anchors.centerIn: parent
                    width: 300
                    visible: table.rows.length === 0
                    title: qsTr("Nothing to show")
                }
            }
        }
    }
}

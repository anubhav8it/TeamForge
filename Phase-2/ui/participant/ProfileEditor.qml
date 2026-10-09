pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// The participant's profile in four guided steps: profile, skills, experience (levels, a
// one-line summary and learning interests), ready. First-time onboarding and later edits use the same editor. The draft is
// form state only; Backend.saveMyProfile validates and stores it, and Backend.profileCompletion
// scores the draft, so the rules live in C++.
Item {
    id: editor

    property bool editing: false
    signal finished(bool created)
    signal claimed()
    signal cancelled()

    property int step: 0
    property bool showErrors: false
    property string draftName: ""
    property string draftProgram: ""
    property string draftSummary: ""
    property var draftInterests: []

    readonly property var steps: [qsTr("Profile"), qsTr("Skills"), qsTr("Experience"), qsTr("Ready")]
    readonly property var stepIntros: [
        qsTr("Start with your name and academic details."),
        qsTr("Add what you can do. Type any skill: new ones are added to TeamForge."),
        qsTr("How strong is each skill? Sum yourself up in one line and say what you want to learn."),
        qsTr("Check your profile. You can edit it any time.")
    ]
    // Recomputed whenever the draft changes (all inputs are referenced via draft()).
    readonly property var completion: Backend.profileCompletion(draft())

    ListModel { id: draftSkills }

    function skillList() {
        const list = []
        for (let i = 0; i < draftSkills.count; ++i)
            list.push({ skill: draftSkills.get(i).skill, level: draftSkills.get(i).level })
        return list
    }
    function draft() {
        // Reference every input so bindings on draft() update.
        void [draftName, draftProgram, draftSummary, draftInterests, draftSkills.count, skillsRevision]
        return { name: draftName.trim(), program: draftProgram.trim(), summary: draftSummary.trim(),
                 skillsOffered: skillList(), skillsWanted: draftInterests }
    }
    property int skillsRevision: 0

    function load() {
        step = 0
        showErrors = false
        draftSkills.clear()
        const me = Backend.myProfile
        if (editing && me.id !== undefined) {
            draftName = me.name
            draftProgram = me.program ?? ""
            draftSummary = me.summary ?? ""
            draftInterests = me.skillsWanted.slice()
            for (const s of me.rankedSkills)
                draftSkills.append({ skill: s.skill, level: s.level })
        } else {
            draftName = ""
            draftProgram = ""
            draftSummary = ""
            draftInterests = []
        }
        skillsRevision++
    }
    function hasSkill(name) {
        const key = name.trim().toLowerCase().replace(/\s+/g, " ")
        for (let i = 0; i < draftSkills.count; ++i) {
            if (draftSkills.get(i).skill.toLowerCase().replace(/\s+/g, " ") === key)
                return true
        }
        return false
    }
    function addSkill(name) {
        const trimmed = name.trim()
        if (trimmed === "" || hasSkill(trimmed))
            return false
        draftSkills.append({ skill: trimmed, level: 3 })
        skillsRevision++
        return true
    }
    function addInterest(name) {
        const key = name.trim()
        if (key === "" || draftInterests.some(s => s.toLowerCase() === key.toLowerCase()))
            return
        draftInterests = draftInterests.concat([key])
    }
    function blocker(index) {
        if (index === 0 && draftName.trim() === "") return qsTr("Add your name to continue.")
        if (index === 1 && draftSkills.count === 0) return qsTr("Add at least one skill to continue.")
        return ""
    }
    function next() {
        if (blocker(step) !== "") {
            showErrors = true
            return
        }
        showErrors = false
        if (step < steps.length - 1)
            step++
        else
            finish()
    }
    function finish() {
        for (let i = 0; i < 2; ++i) {
            if (blocker(i) !== "") {
                step = i
                showErrors = true
                return
            }
        }
        const created = !editing
        if (Backend.saveMyProfile(draft()))
            finished(created)
    }

    onVisibleChanged: if (visible) load()
    onEditingChanged: if (visible) load()
    Component.onCompleted: if (visible) load()

    ProfilePicker {
        id: picker
        onPicked: editor.claimed()
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: Math.min(parent.width - 48, 900)
            x: (parent.width - width) / 2
            spacing: 18

            Item { Layout.preferredHeight: 6 }

            // Heading and completion
            RowLayout {
                Layout.fillWidth: true
                spacing: 18
                ColumnLayout {
                    spacing: 4
                    Layout.fillWidth: true
                    TfText {
                        text: editor.editing ? qsTr("Edit your profile") : qsTr("Create your profile")
                        variant: "eyebrow"
                    }
                    TfText {
                        text: editor.steps[editor.step]
                        font.pixelSize: 30
                        font.weight: Font.ExtraBold
                        font.letterSpacing: -0.6
                    }
                    TfText {
                        text: editor.stepIntros[editor.step]
                        variant: "secondary"
                        font.pixelSize: Theme.fontMd
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
                CoverageRing {
                    size: 84
                    value: editor.completion.completion ?? 0
                    fillColor: (editor.completion.completion ?? 0) >= 1 ? Theme.success : Theme.info
                    caption: qsTr("complete")
                }
            }

            // Step indicator: 01 ... 05, done steps checked.
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: editor.steps
                    delegate: RowLayout {
                        id: stepItem
                        required property string modelData
                        required property int index
                        readonly property bool current: index === editor.step
                        readonly property bool done: index < editor.step
                        // Any step is reachable while editing; during onboarding, earlier ones only.
                        readonly property bool reachable: editor.editing || index <= editor.step
                        spacing: 8
                        Layout.fillWidth: true
                        Rectangle {
                            implicitWidth: 34
                            implicitHeight: 34
                            radius: 17
                            color: stepItem.current ? Theme.info : stepItem.done ? Theme.successSubtle : "transparent"
                            border.width: stepItem.current || stepItem.done ? 0 : 1.5
                            border.color: Theme.borderStrong
                            Behavior on color { ColorAnimation { duration: Theme.animNormal } }
                            Text {
                                anchors.centerIn: parent
                                text: stepItem.done ? "✓" : String(stepItem.index + 1).padStart(2, "0")
                                font.family: Theme.fontFamily
                                font.pixelSize: 13
                                font.weight: Font.ExtraBold
                                color: stepItem.current ? "#0B1A2A" : stepItem.done ? Theme.success : Theme.textSecondary
                            }
                        }
                        TfText {
                            text: stepItem.modelData
                            font.weight: Font.ExtraBold
                            font.pixelSize: Theme.fontSm
                            color: stepItem.current ? Theme.text : Theme.textMuted
                            Layout.fillWidth: true
                        }
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Step %1: %2").arg(stepItem.index + 1).arg(stepItem.modelData)
                        Accessible.onPressAction: if (stepItem.reachable) editor.step = stepItem.index
                        HoverHandler { enabled: stepItem.reachable; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            enabled: stepItem.reachable
                            onTapped: editor.step = stepItem.index
                        }
                    }
                }
            }
            MeterBar {
                Layout.fillWidth: true
                value: (editor.step + 1) / editor.steps.length
                fillColor: Theme.info
            }

            // The step itself
            Rectangle {
                Layout.fillWidth: true
                // Sized to the current step: a StackLayout reports its tallest page.
                implicitHeight: ((stepStack.children[stepStack.currentIndex] as Item)?.implicitHeight ?? 0) + 48
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.border

                StackLayout {
                    id: stepStack
                    anchors.fill: parent
                    anchors.margins: 24
                    currentIndex: editor.step

                    // 01 Profile
                    ColumnLayout {
                        spacing: 14
                        TfText { text: qsTr("Full name"); variant: "label" }
                        TfTextField {
                            text: editor.draftName
                            placeholderText: qsTr("Your full name")
                            font.pixelSize: Theme.fontLg
                            font.weight: Font.Bold
                            implicitHeight: 46
                            Layout.fillWidth: true
                            onTextChanged: if (text !== editor.draftName) editor.draftName = text
                            onAccepted: editor.next()
                        }
                        TfText { text: qsTr("Academic details"); variant: "label"; Layout.topMargin: 4 }
                        TfTextField {
                            text: editor.draftProgram
                            placeholderText: qsTr("Programme and year, e.g. B.Tech CSE · 2nd year")
                            Layout.fillWidth: true
                            onTextChanged: if (text !== editor.draftProgram) editor.draftProgram = text
                            onAccepted: editor.next()
                        }
                        Rectangle {
                            visible: !editor.editing
                            Layout.fillWidth: true
                            Layout.topMargin: 8
                            implicitHeight: claimRow.implicitHeight + 24
                            radius: Theme.radiusSmall
                            color: Theme.infoSubtle
                            RowLayout {
                                id: claimRow
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 12
                                TfText {
                                    text: qsTr("Already in the TeamForge directory? Use your existing profile instead.")
                                    font.weight: Font.Bold
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                TfButton {
                                    text: qsTr("Find my profile")
                                    compact: true
                                    onClicked: picker.open()
                                }
                                TfButton {
                                    text: qsTr("Use demo participant")
                                    variant: "ghost"
                                    compact: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("Explore TeamForge as %1, a ready-made profile").arg(Backend.demoIdentities[0]?.name ?? "")
                                    onClicked: if (Backend.useDemoParticipant()) editor.claimed()
                                }
                            }
                        }
                    }

                    // 02 Skills
                    ColumnLayout {
                        spacing: 14
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            SkillField {
                                id: newSkill
                                Layout.fillWidth: true
                                placeholderText: qsTr("Type a skill, e.g. Python, ROS2, Figma")
                                onAccepted: if (editor.addSkill(text)) clear()
                            }
                            TfButton {
                                text: qsTr("+ Add skill")
                                variant: "primary"
                                enabled: newSkill.text.trim() !== ""
                                onClicked: if (editor.addSkill(newSkill.text)) newSkill.clear()
                            }
                        }
                        TfText {
                            text: draftSkills.count > 0 ? qsTr("Your skills") : qsTr("No skills yet")
                            variant: "label"
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            Repeater {
                                model: draftSkills
                                delegate: Rectangle {
                                    id: skillPill
                                    required property int index
                                    required property string skill
                                    implicitWidth: pillRow.implicitWidth + 24
                                    implicitHeight: 34
                                    radius: 17
                                    color: Theme.infoSubtle
                                    border.color: Qt.rgba(Theme.info.r, Theme.info.g, Theme.info.b, 0.45)
                                    Row {
                                        id: pillRow
                                        anchors.centerIn: parent
                                        spacing: 8
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: Theme.skill(skillPill.skill)
                                            textFormat: Text.PlainText
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSm
                                            font.weight: Font.ExtraBold
                                            font.letterSpacing: 0.4
                                            color: Theme.text
                                        }
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: "✕"
                                            font.pixelSize: 12
                                            color: removeHover.hovered ? Theme.accentHover : Theme.textSecondary
                                            Accessible.role: Accessible.Button
                                            Accessible.name: qsTr("Remove %1").arg(skillPill.skill)
                                            Accessible.onPressAction: { draftSkills.remove(skillPill.index); editor.skillsRevision++ }
                                            HoverHandler { id: removeHover; cursorShape: Qt.PointingHandCursor }
                                            TapHandler { onTapped: { draftSkills.remove(skillPill.index); editor.skillsRevision++ } }
                                        }
                                    }
                                }
                            }
                        }
                        TfText {
                            text: qsTr("Popular on TeamForge")
                            variant: "label"
                            Layout.topMargin: 6
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            Repeater {
                                model: editor.visible && editor.step === 1
                                       ? Backend.popularSkills(30).filter(p => editor.skillsRevision >= 0 && !editor.hasSkill(p.skill)).slice(0, 16)
                                       : []
                                delegate: Chip {
                                    id: quickPick
                                    required property var modelData
                                    skill: true
                                    text: "+ " + modelData.skill
                                    tone: pickHover.hovered ? "info" : "muted"
                                    Accessible.role: Accessible.Button
                                    Accessible.name: qsTr("Add %1").arg(modelData.skill)
                                    Accessible.onPressAction: editor.addSkill(quickPick.modelData.skill)
                                    HoverHandler { id: pickHover; cursorShape: Qt.PointingHandCursor }
                                    TapHandler { onTapped: editor.addSkill(quickPick.modelData.skill) }
                                }
                            }
                        }
                    }

                    // 03 Experience
                    ColumnLayout {
                        spacing: 0
                        Repeater {
                            model: draftSkills
                            delegate: Rectangle {
                                id: levelRow
                                required property int index
                                required property string skill
                                required property int level
                                Layout.fillWidth: true
                                implicitHeight: 50
                                color: "transparent"
                                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
                                RowLayout {
                                    anchors.fill: parent
                                    spacing: 16
                                    TfText {
                                        text: Theme.skill(levelRow.skill)
                                        font.weight: Font.ExtraBold
                                        font.letterSpacing: 0.4
                                        Layout.fillWidth: true
                                    }
                                    LevelPips {
                                        interactive: true
                                        level: levelRow.level
                                        onPicked: newLevel => { draftSkills.setProperty(levelRow.index, "level", newLevel); editor.skillsRevision++ }
                                    }
                                    TfText {
                                        text: Theme.levelName(levelRow.level)
                                        variant: "secondary"
                                        font.weight: Font.Bold
                                        Layout.preferredWidth: 124
                                    }
                                }
                            }
                        }
                        TfText {
                            text: qsTr("One-line summary")
                            variant: "label"
                            Layout.topMargin: 22
                            Layout.bottomMargin: 8
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            TfTextField {
                                id: summaryField
                                text: editor.draftSummary
                                maximumLength: 120
                                placeholderText: qsTr("e.g. AI/ML student focused on Python, NLP and data analysis.")
                                Layout.fillWidth: true
                                onTextChanged: if (text !== editor.draftSummary) editor.draftSummary = text
                            }
                            TfButton {
                                text: qsTr("Suggest")
                                enabled: draftSkills.count > 0
                                ToolTip.visible: hovered
                                ToolTip.delay: 400
                                ToolTip.text: qsTr("Write a sentence from your strongest skills")
                                onClicked: editor.draftSummary = Backend.composeSummary(editor.skillList())
                            }
                        }
                        TfText {
                            text: qsTr("Skills and experience only, in one sentence.")
                            variant: "muted"
                            Layout.topMargin: 6
                        }
                        TfText {
                            text: qsTr("Learning interests")
                            variant: "label"
                            Layout.topMargin: 22
                            Layout.bottomMargin: 8
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            SkillField {
                                id: newInterest
                                Layout.fillWidth: true
                                placeholderText: qsTr("What do you want to learn? e.g. Kubernetes")
                                onAccepted: { editor.addInterest(text); clear() }
                            }
                            TfButton {
                                text: qsTr("+ Add")
                                enabled: newInterest.text.trim() !== ""
                                onClicked: { editor.addInterest(newInterest.text); newInterest.clear() }
                            }
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            Repeater {
                                model: editor.draftInterests
                                delegate: Chip {
                                    id: interest
                                    required property string modelData
                                    skill: true
                                    text: modelData + "  ✕"
                                    tone: "muted"
                                    Accessible.role: Accessible.Button
                                    Accessible.name: qsTr("Remove %1").arg(modelData)
                                    Accessible.onPressAction: editor.draftInterests = editor.draftInterests.filter(s => s !== interest.modelData)
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                    TapHandler { onTapped: editor.draftInterests = editor.draftInterests.filter(s => s !== interest.modelData) }
                                }
                            }
                        }
                    }

                    // 04 Ready
                    ColumnLayout {
                        spacing: 16
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 18
                            Avatar {
                                name: editor.draftName || "?"
                                seed: Backend.myProfileId !== "" ? Backend.myProfileId : editor.draftName
                                size: 76
                                Layout.alignment: Qt.AlignTop
                            }
                            ColumnLayout {
                                spacing: 4
                                Layout.fillWidth: true
                                TfText {
                                    text: editor.draftName
                                    font.pixelSize: 26
                                    font.weight: Font.ExtraBold
                                    Layout.fillWidth: true
                                }
                                TfText {
                                    visible: editor.draftProgram !== ""
                                    text: editor.draftProgram
                                    variant: "muted"
                                    font.weight: Font.Bold
                                    Layout.fillWidth: true
                                }
                                TfText {
                                    visible: editor.draftSummary !== ""
                                    text: editor.draftSummary
                                    variant: "secondary"
                                    font.pixelSize: Theme.fontMd
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                SkillChips {
                                    Layout.fillWidth: true
                                    Layout.topMargin: 6
                                    skills: editor.skillsRevision >= 0 ? editor.skillList().sort((a, b) => b.level - a.level) : []
                                    maxVisible: 6
                                }
                            }
                        }
                        TfText { text: qsTr("Profile checklist"); variant: "label" }
                        // Compact, so the Finish button stays in view on small windows.
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            Repeater {
                                model: editor.completion.checklist ?? []
                                delegate: Chip {
                                    required property var modelData
                                    large: true
                                    text: (modelData.done ? "✓  " : "○  ") + modelData.label
                                    tone: modelData.done ? "success" : "muted"
                                }
                            }
                        }
                    }
                }
            }

            // Navigation
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                TfButton {
                    text: qsTr("Back")
                    variant: "ghost"
                    enabled: editor.step > 0
                    onClicked: { editor.showErrors = false; editor.step-- }
                }
                TfButton {
                    visible: editor.editing
                    text: qsTr("Cancel")
                    variant: "ghost"
                    onClicked: editor.cancelled()
                }
                TfText {
                    visible: editor.showErrors && editor.blocker(editor.step) !== ""
                    text: editor.blocker(editor.step)
                    color: Theme.warning
                    font.weight: Font.Bold
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                }
                Item { Layout.fillWidth: true; visible: !(editor.showErrors && editor.blocker(editor.step) !== "") }
                TfButton {
                    visible: editor.editing && editor.step < editor.steps.length - 1
                    text: qsTr("Save profile")
                    onClicked: editor.finish()
                }
                TfButton {
                    text: editor.step < editor.steps.length - 1 ? qsTr("Continue") : editor.editing ? qsTr("Save profile") : qsTr("Finish")
                    variant: "primary"
                    Layout.preferredWidth: 150
                    onClicked: editor.next()
                }
            }

            Item { Layout.preferredHeight: 20 }
        }
    }
}

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
    signal message(string text)

    property int step: 0
    property bool showErrors: false
    property string draftName: ""
    property string draftProgram: ""
    property string draftSummary: ""
    property var draftInterests: []
    // Profile details ({projects, education, certifications, achievements, skillYears,
    // totalYears}), usually added from a resume; saved with the profile.
    property var draftDetails: ({})
    // Profile photo chosen in step 1 (saved to the TeamForge server with the profile).
    property string draftPhoto: ""
    property bool photoRemoved: false

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
        void [draftName, draftProgram, draftSummary, draftInterests, draftDetails, draftSkills.count, skillsRevision]
        return { name: draftName.trim(), program: draftProgram.trim(), summary: draftSummary.trim(),
                 skillsOffered: skillList(), skillsWanted: draftInterests, details: draftDetails }
    }
    property int skillsRevision: 0

    function load() {
        step = 0
        showErrors = false
        draftPhoto = ""
        photoRemoved = false
        draftSkills.clear()
        const me = Backend.myProfile
        if (editing && me.id !== undefined) {
            draftName = me.name
            draftProgram = me.program ?? ""
            draftSummary = me.summary ?? ""
            draftInterests = me.skillsWanted.slice()
            for (const s of me.rankedSkills)
                draftSkills.append({ skill: s.skill, level: s.level })
            draftDetails = Object.assign({}, me.details ?? {})
        } else {
            draftName = ""
            draftProgram = ""
            draftSummary = ""
            draftInterests = []
            draftDetails = {}
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
    // What the resume review applied (ResumeReviewDrawer.apply): fills the draft only; the
    // profile is saved with the usual Save / Finish.
    function applyResume(changes) {
        if (changes.name !== "")
            draftName = changes.name
        let added = 0
        for (const s of changes.skills) {
            let found = -1
            for (let i = 0; i < draftSkills.count; ++i) {
                if (draftSkills.get(i).skill.toLowerCase().replace(/\s+/g, " ") === s.skill.toLowerCase().replace(/\s+/g, " "))
                    found = i
            }
            if (found >= 0) {
                draftSkills.setProperty(found, "level", s.level)
            } else {
                draftSkills.append({ skill: s.skill, level: s.level })
                ++added
            }
        }
        const details = Object.assign({}, draftDetails)
        for (const key of ["projects", "education", "certifications", "achievements"]) {
            const merged = (details[key] ?? []).slice()
            for (const item of changes[key] ?? [])
                if (!merged.some(m => m.toLowerCase() === item.toLowerCase()))
                    merged.push(item)
            details[key] = merged
        }
        details.skillYears = Object.assign({}, details.skillYears ?? {}, changes.skillYears)
        if (changes.totalYears >= 0)
            details.totalYears = changes.totalYears
        draftDetails = details
        skillsRevision++
        message(qsTr("Added from your resume: %1. Check each step, then save.")
                .arg(Theme.count(added, qsTr("new skill"), qsTr("new skills"))))
    }
    function yearsIn(skill) {
        void draftDetails
        const years = (draftDetails.skillYears ?? {})[skill.toLowerCase()] ?? (draftDetails.skillYears ?? {})[skill]
        return years === undefined ? -1 : years
    }
    function detailCount(key) {
        return (draftDetails[key] ?? []).length
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
        if (Backend.saveMyProfile(draft())) {
            // One identity: the account's name, academic details and photo follow this profile.
            if (Hub.connected) {
                Hub.syncIdentity(draftName.trim(), draftProgram.trim(), Backend.myProfileId)
                if (draftPhoto !== "")
                    Hub.uploadPhoto(draftPhoto)
                else if (photoRemoved)
                    Hub.removePhoto()
            }
            finished(created)
        }
    }

    onVisibleChanged: if (visible) load()
    onEditingChanged: if (visible) load()
    Component.onCompleted: if (visible) load()

    ProfilePicker {
        id: picker
        onPicked: {
            Hub.syncIdentity(Backend.myProfile.name ?? "", Backend.myProfile.program ?? "", Backend.myProfileId)
            editor.claimed()
        }
    }
    ResumeReviewDrawer {
        id: resumeReview
        draftSkills: editor.skillsRevision >= 0 ? editor.skillList() : []
        onApplied: changes => editor.applyResume(changes)
    }

    ScrollView {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: navBar.top
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: Math.min(parent.width - (editor.width < 520 ? 24 : 48), 900)
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
                        font.pixelSize: 36
                        font.weight: Font.ExtraBold
                        font.letterSpacing: -0.9
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

            // Step rail: the four steps on one line that fills as the person moves on; the
            // current step glows, finished ones are ticked. Any step is reachable while editing;
            // during onboarding, earlier ones only.
            Item {
                id: rail
                Layout.fillWidth: true
                Layout.topMargin: 4
                implicitHeight: 64
                readonly property real spacingX: width / editor.steps.length
                readonly property real firstX: spacingX / 2

                Rectangle {
                    x: rail.firstX
                    y: 17
                    width: rail.width - rail.spacingX
                    height: 4
                    radius: 2
                    color: Theme.input
                    border.color: Theme.border
                }
                Rectangle {
                    x: rail.firstX
                    y: 17
                    width: (rail.width - rail.spacingX) * (editor.step / (editor.steps.length - 1))
                    height: 4
                    radius: 2
                    visible: width > 0
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Theme.success }
                        GradientStop { position: 1.0; color: Theme.info }
                    }
                    Behavior on width { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.OutCubic } }
                }

                Repeater {
                    model: editor.steps
                    delegate: Item {
                        id: stepItem
                        required property string modelData
                        required property int index
                        readonly property bool current: index === editor.step
                        readonly property bool done: index < editor.step
                        readonly property bool reachable: editor.editing || index <= editor.step
                        x: rail.spacingX * index
                        width: rail.spacingX
                        height: rail.height
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Step %1: %2").arg(stepItem.index + 1).arg(stepItem.modelData)
                        Accessible.onPressAction: if (stepItem.reachable) editor.step = stepItem.index

                        // halo behind the current step
                        Rectangle {
                            anchors.horizontalCenter: dot.horizontalCenter
                            anchors.verticalCenter: dot.verticalCenter
                            width: 50
                            height: 50
                            radius: 25
                            color: Qt.rgba(Theme.info.r, Theme.info.g, Theme.info.b, 0.18)
                            visible: stepItem.current
                        }
                        Rectangle {
                            id: dot
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: 1
                            width: 36
                            height: 36
                            radius: 18
                            color: stepItem.done ? "#12352C" : Theme.surfaceRaised
                            gradient: stepItem.current ? currentFill : null
                            border.width: stepItem.current ? 0 : 1.5
                            border.color: stepItem.done ? Theme.success : stepHover.hovered && stepItem.reachable ? Theme.textSecondary : Theme.borderStrong
                            scale: stepItem.current ? 1.06 : 1
                            Behavior on scale { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutBack } }
                            Gradient {
                                id: currentFill
                                GradientStop { position: 0.0; color: Qt.lighter(Theme.info, 1.15) }
                                GradientStop { position: 1.0; color: "#3B82F6" }
                            }
                            Text {
                                anchors.centerIn: parent
                                text: stepItem.done ? "✓" : String(stepItem.index + 1).padStart(2, "0")
                                font.family: Theme.fontFamily
                                font.pixelSize: 13
                                font.weight: Font.ExtraBold
                                color: stepItem.current ? "#06121F" : stepItem.done ? Theme.success : Theme.textSecondary
                            }
                        }
                        TfText {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: dot.bottom
                            anchors.topMargin: 8
                            text: stepItem.modelData
                            font.weight: Font.ExtraBold
                            font.pixelSize: Theme.fontSm
                            color: stepItem.current ? Theme.text : stepItem.done ? Theme.textSecondary : Theme.textMuted
                        }
                        HoverHandler { id: stepHover; enabled: stepItem.reachable; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            enabled: stepItem.reachable
                            onTapped: editor.step = stepItem.index
                        }
                    }
                }
            }

            // The step itself
            Surface {
                Layout.fillWidth: true
                // Sized to the current step: a StackLayout reports its tallest page.
                implicitHeight: ((stepStack.children[stepStack.currentIndex] as Item)?.implicitHeight ?? 0) + (editor.width < 520 ? 28 : 56)
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.border

                StackLayout {
                    id: stepStack
                    anchors.fill: parent
                    anchors.margins: editor.width < 520 ? 14 : 28
                    currentIndex: editor.step

                    // 01 Profile
                    ColumnLayout {
                        spacing: 14
                        ResumeImport {
                            Layout.fillWidth: true
                            Layout.bottomMargin: 4
                            onReviewRequested: resumeReview.open()
                        }
                        PhotoPicker {
                            Layout.fillWidth: true
                            Layout.bottomMargin: 4
                            compact: true
                            size: 96
                            name: editor.draftName !== "" ? editor.draftName : qsTr("You")
                            seed: editor.editing ? Backend.myProfileId : ""
                            pending: editor.draftPhoto
                            removedPending: editor.photoRemoved
                            onCropped: url => { editor.draftPhoto = url; editor.photoRemoved = false }
                            onRemoved: { editor.draftPhoto = ""; editor.photoRemoved = true }
                        }
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
                            GridLayout {
                                id: claimRow
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: 12
                                columns: editor.width < 640 ? 2 : 3
                                columnSpacing: 12
                                rowSpacing: 8
                                TfText {
                                    text: qsTr("Already in the TeamForge directory? Use your existing profile instead.")
                                    font.weight: Font.Bold
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                    Layout.columnSpan: editor.width < 640 ? 2 : 1
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
                        MotivationQuote {
                            moment: "register"
                            Layout.fillWidth: true
                            Layout.topMargin: 8
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
                        // Encouragement for the skills chosen so far; changes with the selection.
                        MotivationQuote {
                            moment: "skills"
                            skills: editor.skillsRevision >= 0 ? editor.skillList() : []
                            learningCount: editor.draftInterests.length
                            Layout.fillWidth: true
                            Layout.topMargin: 4
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
                                    // Years the resume states for this skill, when it was imported.
                                    TfText {
                                        readonly property real years: editor.yearsIn(levelRow.skill)
                                        visible: years >= 0
                                        text: qsTr("%1 yrs").arg(years)
                                        variant: "muted"
                                        font.weight: Font.Bold
                                        Layout.preferredWidth: 64
                                        ToolTip.visible: yearsHover.hovered
                                        ToolTip.delay: 400
                                        ToolTip.text: qsTr("Experience stated in your resume")
                                        HoverHandler { id: yearsHover }
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
                        // Follows the levels and learning interests set above.
                        MotivationQuote {
                            moment: "skills"
                            skills: editor.skillsRevision >= 0 ? editor.skillList() : []
                            learningCount: editor.draftInterests.length
                            Layout.fillWidth: true
                            Layout.topMargin: 24
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
                        // Details from the resume, at a glance.
                        Flow {
                            visible: editor.detailCount("projects") + editor.detailCount("education")
                                     + editor.detailCount("certifications") + editor.detailCount("achievements") > 0
                                     || (editor.draftDetails.totalYears ?? -1) >= 0
                            Layout.fillWidth: true
                            spacing: 8
                            Repeater {
                                model: [[editor.detailCount("projects"), qsTr("project"), qsTr("projects")],
                                        [editor.detailCount("education"), qsTr("qualification"), qsTr("qualifications")],
                                        [editor.detailCount("certifications"), qsTr("certification"), qsTr("certifications")],
                                        [editor.detailCount("achievements"), qsTr("achievement"), qsTr("achievements")]]
                                delegate: Chip {
                                    required property var modelData
                                    visible: modelData[0] > 0
                                    large: true
                                    tone: "info"
                                    text: Theme.count(modelData[0], modelData[1], modelData[2])
                                }
                            }
                            Chip {
                                visible: (editor.draftDetails.totalYears ?? -1) >= 0
                                large: true
                                tone: "warning"
                                text: qsTr("%1 years experience").arg(editor.draftDetails.totalYears ?? 0)
                            }
                        }
                        TfText { text: qsTr("Profile checklist"); variant: "label" }
                        // Compact, so the profile fits on small windows.
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

            Item { Layout.preferredHeight: 20 }
        }
    }

    // Navigation, pinned under the steps so Back / Continue / Finish are always in view however
    // long a step (or its motivational quote) gets.
    Rectangle {
        id: navBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        implicitHeight: navRow.implicitHeight + 32
        // Glass over the page: content scrolling underneath fades into it.
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.82) }
            GradientStop { position: 1.0; color: Theme.background }
        }
        Rectangle {
            width: parent.width
            height: 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.5; color: Theme.borderStrong }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        RowLayout {
            id: navRow
            width: Math.min(parent.width - (editor.width < 520 ? 24 : 48), 900)
            x: (parent.width - width) / 2
            anchors.verticalCenter: parent.verticalCenter
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
            TfText {
                visible: !(editor.showErrors && editor.blocker(editor.step) !== "") && editor.width >= 520
                text: qsTr("Step %1 of %2").arg(editor.step + 1).arg(editor.steps.length)
                variant: "muted"
                font.weight: Font.Bold
            }
            TfButton {
                text: editor.step < editor.steps.length - 1 ? qsTr("Continue") : editor.editing ? qsTr("Save profile") : qsTr("Finish")
                variant: "primary"
                Layout.preferredWidth: editor.width < 520 ? 130 : 170
                onClicked: editor.next()
            }
        }
    }
}

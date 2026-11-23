pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import TeamForge

// Create a team, or edit one (leader and team admins). The creator becomes its leader.
HubDialog {
    id: form

    property var team: null          // editing when set
    property var events: []          // [{id, name}] for the optional event link
    property var required: []        // [{ skill, level }]: what the team needs, with minimum levels
    signal saved(var team)

    function addSkill() {
        const name = newSkill.text.trim().toLowerCase().replace(/\s+/g, " ")
        if (name === "")
            return
        if (required.length >= 15) {
            error = qsTr("At most 15 required skills.")
            return
        }
        if (!required.some(r => r.skill === name))
            required = required.concat([{ skill: name, level: 2 }])
        newSkill.text = ""
    }
    function setLevel(skill, level) {
        required = required.map(r => r.skill === skill ? { skill: skill, level: level } : r)
    }
    // { skill: level } for the server. Built by hand: QML's JavaScript engine has no Object.fromEntries,
    // and calling it threw before the request was sent, so "Create team" silently did nothing.
    function requiredSkillsMap() {
        const map = {}
        for (const r of required)
            map[r.skill] = r.level
        return map
    }

    title: team ? qsTr("Edit team") : qsTr("Create a team")
    subtitle: team ? "" : qsTr("You become the team's leader. People can then apply to join, and members get a private chat.")
    primaryText: team ? qsTr("Save") : qsTr("Create team")
    primaryEnabled: name.text.trim().length >= 3

    function openFor(existing) {
        team = existing
        name.text = existing ? existing.name : ""
        tagline.text = existing ? existing.tagline : ""
        description.text = existing ? existing.description : ""
        objectives.text = existing ? existing.objectives : ""
        roles.text = existing ? existing.openRoles.join(", ") : ""
        required = existing ? Object.keys(existing.requiredSkills ?? {}).map(s => ({ skill: s, level: existing.requiredSkills[s] })) : []
        newSkill.text = ""
        size.value = existing ? existing.maxMembers : 4
        recruiting.checked = existing ? existing.recruiting : true
        Hub.get("/events?pageSize=50", (status, data) => {
            if (status !== 200)
                return
            form.events = data.items.map(e => ({ id: e.id, name: e.name }))
            const at = existing && existing.event ? form.events.findIndex(e => e.id === existing.event.id) : -1
            eventBox.currentIndex = at + 1
        })
        open()
    }

    onAccepted: {
        const problem = Hub.checkText(description.text, 2000, qsTr("Description")) || Hub.checkText(objectives.text, 2000, qsTr("Objectives"))
        if (problem !== "") {
            error = problem
            return
        }
        const body = {
            name: name.text, tagline: tagline.text, description: description.text, objectives: objectives.text,
            openRoles: roles.text, requiredSkills: form.requiredSkillsMap(),
            maxMembers: size.value, recruiting: recruiting.checked,
            eventId: eventBox.currentIndex > 0 ? form.events[eventBox.currentIndex - 1].id : null
        }
        busy = true
        Hub.post(team ? "/teams/" + team.id : "/teams", body, (status, data) => {
            busy = false
            if (status === 200 || status === 201) {
                close()
                form.saved(data.team)
            } else {
                error = data.error ?? qsTr("The team could not be saved.")
            }
        })
    }

    HubLabel { text: qsTr("Team name"); hint: qsTr("3 to 60 characters; must be unique.") }
    TfTextField { id: name; placeholderText: qsTr("e.g. Rocket Lab"); maximumLength: 60; Layout.fillWidth: true }
    HubLabel { text: qsTr("Tagline") }
    TfTextField { id: tagline; placeholderText: qsTr("One line about the team"); maximumLength: 120; Layout.fillWidth: true }
    HubLabel { text: qsTr("About the team") }
    TfTextArea { id: description; maxLength: 2000; placeholderText: qsTr("What you are building and how you work"); Layout.fillWidth: true }
    HubLabel { text: qsTr("Objectives") }
    TfTextArea { id: objectives; maxLength: 2000; placeholderText: qsTr("What the team wants to achieve"); Layout.fillWidth: true }
    HubLabel { text: qsTr("Open roles"); hint: qsTr("Separate with commas, e.g. Frontend, Tester") }
    TfTextField { id: roles; Layout.fillWidth: true }
    HubLabel {
        text: qsTr("Skills the team needs")
        hint: qsTr("With the minimum level for each. Used for skill coverage and to show how well people fit.")
    }
    Repeater {
        model: form.required
        delegate: RowLayout {
            id: skillRow
            required property var modelData
            Layout.fillWidth: true
            spacing: 10
            TfText { text: Theme.skill(skillRow.modelData.skill); font.weight: Font.Bold; Layout.fillWidth: true; Layout.minimumWidth: 0 }
            TfText {
                visible: form.width > 420
                text: skillRow.modelData.level === 1 ? qsTr("Any level") : Theme.levelName(skillRow.modelData.level)
                variant: "muted"
                font.pixelSize: Theme.fontXs
            }
            LevelPips {
                level: skillRow.modelData.level
                interactive: true
                segmentWidth: 18
                onPicked: level => form.setLevel(skillRow.modelData.skill, level)
            }
            TfButton {
                text: "✕"
                compact: true
                variant: "ghost"
                Accessible.name: qsTr("Remove %1").arg(skillRow.modelData.skill)
                onClicked: form.required = form.required.filter(r => r.skill !== skillRow.modelData.skill)
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        SkillField {
            id: newSkill
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            onAccepted: form.addSkill()
        }
        TfButton { text: qsTr("Add"); compact: true; enabled: newSkill.text.trim() !== ""; onClicked: form.addSkill() }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 16
        ColumnLayout {
            spacing: 4
            HubLabel { text: qsTr("Team size") }
            TfSpinBox { id: size; from: 2; to: 20; value: 4 }
        }
        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true
            HubLabel { text: qsTr("Event (optional)") }
            TfComboBox {
                id: eventBox
                model: [qsTr("No event")].concat(form.events.map(e => e.name))
                Layout.fillWidth: true
            }
        }
    }
    TfCheck {
        id: recruiting
        text: qsTr("Recruiting: people can apply to join")
        checked: true
        onToggled: value => recruiting.checked = value
    }
}

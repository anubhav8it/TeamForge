pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import TeamForge

// Create a team, or edit one (leader and team admins). The creator becomes its leader.
HubDialog {
    id: form

    property var team: null          // editing when set
    property var events: []          // [{id, name}] for the optional event link
    signal saved(var team)

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
        skills.text = existing ? existing.skills.join(", ") : ""
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
            openRoles: roles.text, skills: skills.text, maxMembers: size.value, recruiting: recruiting.checked,
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
    HubLabel { text: qsTr("Skills you use"); hint: qsTr("Separate with commas") }
    TfTextField { id: skills; Layout.fillWidth: true }
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

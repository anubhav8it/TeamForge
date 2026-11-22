pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import TeamForge

// Apply to join a team (kind "team") or to take part in an event (kind "event"). The reviewer
// also sees the applicant's Community profile. One active application per team or event: the
// server refuses a second one.
HubDialog {
    id: apply

    property string kind: "team"
    property string targetId
    property string targetName
    property var positions: []
    signal applied(string status)

    title: kind === "team" ? qsTr("Apply to join %1").arg(targetName) : qsTr("Apply for %1").arg(targetName)
    subtitle: kind === "team" ? qsTr("The team leader sees this together with your profile and decides.")
                              : qsTr("The organiser sees this together with your profile. If the event is full you join the waitlist.")
    primaryText: qsTr("Send application")
    primaryEnabled: (kind === "team" ? intro.text.trim() !== "" && reason.text.trim() !== "" : motivation.text.trim() !== "")
                    && (kind !== "event" || positions.length === 0 || position.currentIndex >= 0)

    function openFor(newKind, id, name, eventPositions) {
        kind = newKind
        targetId = id
        targetName = name
        positions = eventPositions ?? []
        for (const field of [intro, reason, experience, interests, motivation, availability])
            field.clear()
        skills.text = (Hub.profile.skills ?? []).join(", ")
        experience.text = Hub.profile.experience ?? ""
        interests.text = Hub.profile.interests ?? ""
        position.currentIndex = positions.length > 0 ? 0 : -1
        open()
    }

    onAccepted: {
        const limit = 1500
        const problem = kind === "team"
            ? Hub.checkText(intro.text, limit, qsTr("Introduction"), true) || Hub.checkText(reason.text, limit, qsTr("Why you want to join"), true)
              || Hub.checkText(experience.text, limit, qsTr("Experience")) || Hub.checkText(interests.text, 1000, qsTr("Interests"))
            : Hub.checkText(motivation.text, limit, qsTr("Why you want to take part"), true)
              || Hub.checkText(experience.text, limit, qsTr("Experience")) || Hub.checkText(availability.text, 300, qsTr("Availability"))
        if (problem !== "") {
            error = problem
            return
        }
        busy = true
        const path = kind === "team" ? "/teams/" + targetId + "/applications" : "/events/" + encodeURIComponent(targetId) + "/applications"
        const body = kind === "team"
            ? { introduction: intro.text, skills: skills.text, experience: experience.text, interests: interests.text, reason: reason.text }
            : { position: positions.length > 0 ? positions[position.currentIndex] : "", motivation: motivation.text,
                experience: experience.text, availability: availability.text }
        Hub.post(path, body, (status, data) => {
            busy = false
            if (status === 201) {
                close()
                apply.applied(data.application.status)
            } else if (data.code === "profile_needed") {
                error = data.error
            } else {
                error = data.error ?? qsTr("The application could not be sent.")
            }
        })
    }

    // team
    HubLabel { visible: apply.kind === "team"; text: qsTr("Introduce yourself"); hint: qsTr("Who you are, in a few sentences.") }
    TfTextArea { id: intro; visible: apply.kind === "team"; maxLength: 1500; Layout.fillWidth: true }
    HubLabel { visible: apply.kind === "team"; text: qsTr("Why do you want to join?") }
    TfTextArea { id: reason; visible: apply.kind === "team"; maxLength: 1500; Layout.fillWidth: true }
    HubLabel { visible: apply.kind === "team"; text: qsTr("Skills"); hint: qsTr("Separate with commas") }
    TfTextField { id: skills; visible: apply.kind === "team"; Layout.fillWidth: true }

    // event
    HubLabel { visible: apply.kind === "event" && apply.positions.length > 0; text: qsTr("Position") }
    TfComboBox {
        id: position
        visible: apply.kind === "event" && apply.positions.length > 0
        model: apply.positions
        Layout.fillWidth: true
    }
    HubLabel { visible: apply.kind === "event"; text: qsTr("Why do you want to take part?") }
    TfTextArea { id: motivation; visible: apply.kind === "event"; maxLength: 1500; Layout.fillWidth: true }

    // both
    HubLabel { text: qsTr("Experience"); hint: qsTr("Projects, courses, events. Optional.") }
    TfTextArea { id: experience; maxLength: 1500; Layout.fillWidth: true }
    HubLabel { visible: apply.kind === "team"; text: qsTr("Interests"); hint: qsTr("Optional") }
    TfTextArea { id: interests; visible: apply.kind === "team"; maxLength: 1000; minimumHeight: 56; Layout.fillWidth: true }
    HubLabel { visible: apply.kind === "event"; text: qsTr("Availability"); hint: qsTr("Optional, e.g. both days, evenings only") }
    TfTextArea { id: availability; visible: apply.kind === "event"; maxLength: 300; minimumHeight: 56; Layout.fillWidth: true }
}

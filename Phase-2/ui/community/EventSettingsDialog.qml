import QtQuick
import QtQuick.Layouts
import TeamForge

// Open a project for applications, or change how it takes them (organiser or admin). The host
// who opens it becomes its organiser and reviews its applications.
HubDialog {
    id: settings

    property var event: ({})
    signal saved(var event)

    title: event.configured ? qsTr("Application settings") : qsTr("Open %1 for applications").arg(event.name ?? "")
    subtitle: event.configured ? "" : qsTr("You become the organiser: applications come to you to approve, waitlist or reject.")
    primaryText: event.configured ? qsTr("Save") : qsTr("Open for applications")

    function openFor(ev) {
        event = ev
        description.text = ev.description ?? ""
        requirements.text = ev.requirements ?? ""
        eligibility.text = ev.eligibility ?? ""
        positions.text = (ev.positions ?? []).join(", ")
        capacity.text = ev.capacity !== null && ev.capacity !== undefined ? String(ev.capacity) : ""
        deadline.text = ev.deadline ? Hub.isoDate(ev.deadline) : ""
        accepting.checked = ev.configured ? ev.accepting : true
        open()
    }

    onAccepted: {
        let seconds = null
        if (deadline.text.trim() !== "") {
            seconds = Hub.parseDate(deadline.text)
            if (seconds <= 0) {
                error = qsTr("Write the deadline as YYYY-MM-DD, e.g. 2026-12-15.")
                return
            }
        }
        const places = capacity.text.trim()
        if (places !== "" && !/^\d+$/.test(places)) {
            error = qsTr("Places must be a whole number, or empty for no limit.")
            return
        }
        busy = true
        Hub.post("/events/" + encodeURIComponent(event.id) + "/settings", {
            description: description.text, requirements: requirements.text, eligibility: eligibility.text,
            positions: positions.text, capacity: places === "" ? null : Number(places), deadline: seconds,
            accepting: accepting.checked
        }, (status, data) => {
            busy = false
            if (status === 200) { close(); settings.saved(data.event) }
            else error = data.error ?? qsTr("The settings could not be saved.")
        })
    }

    HubLabel { text: qsTr("Description"); hint: qsTr("What participants will do. The project's summary and required skills are shown too.") }
    TfTextArea { id: description; maxLength: 3000; Layout.fillWidth: true }
    HubLabel { text: qsTr("Requirements") }
    TfTextArea { id: requirements; maxLength: 2000; minimumHeight: 56; Layout.fillWidth: true }
    HubLabel { text: qsTr("Eligibility"); hint: qsTr("Who may apply, e.g. 2nd year B.Tech") }
    TfTextArea { id: eligibility; maxLength: 1000; minimumHeight: 56; Layout.fillWidth: true }
    HubLabel { text: qsTr("Positions"); hint: qsTr("Optional, separated by commas. Applicants pick one.") }
    TfTextField { id: positions; Layout.fillWidth: true }
    RowLayout {
        Layout.fillWidth: true
        spacing: 16
        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true
            HubLabel { text: qsTr("Places"); hint: qsTr("Empty = no limit. Later applicants join the waitlist.") }
            TfTextField { id: capacity; placeholderText: qsTr("No limit"); inputMethodHints: Qt.ImhDigitsOnly; Layout.fillWidth: true }
        }
        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true
            HubLabel { text: qsTr("Registration deadline"); hint: qsTr("YYYY-MM-DD, end of that day. Empty = none.") }
            TfTextField { id: deadline; placeholderText: qsTr("2026-12-15"); Layout.fillWidth: true }
        }
    }
    TfCheck {
        id: accepting
        text: qsTr("Accepting applications")
        checked: true
        onToggled: value => accepting.checked = value
    }
}

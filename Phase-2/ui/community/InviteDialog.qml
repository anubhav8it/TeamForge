pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import TeamForge

// Confirm a direct invitation, with an optional personal message. The person is notified (and
// emailed if email is set up and they allow it); they join when they accept, if there is still
// a place. The server checks everything again.
HubDialog {
    id: dialog

    property var team: null
    property var person: ({})
    property var report: ({})
    signal invited(var person)

    title: qsTr("Invite %1 to %2").arg(person.name ?? "").arg(team ? team.name : "")
    subtitle: qsTr("They get a notification and can accept or decline. They join when they accept, if there is still a place. The invitation is open for 14 days.")
    primaryText: qsTr("Send invitation")

    function openFor(theTeam, candidate, compatibility) {
        team = theTeam
        person = candidate ?? {}
        report = compatibility ?? {}
        message.clear()
        open()
    }

    onAccepted: {
        const problem = Hub.checkText(message.text, 600, qsTr("Message"))
        if (problem !== "") {
            error = problem
            return
        }
        busy = true
        Hub.post("/teams/" + team.id + "/invitations", { userId: person.id, message: message.text }, (status, data) => {
            busy = false
            if (status === 201) {
                close()
                dialog.invited(person)
            } else {
                error = data.error ?? qsTr("The invitation could not be sent.")
            }
        })
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12
        Avatar { person: dialog.person; size: 48 }
        ColumnLayout {
            spacing: 2
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            TfText { text: dialog.person.name ?? ""; font.weight: Font.Bold; Layout.fillWidth: true }
            TfText {
                text: dialog.report.enoughInformation
                      ? qsTr("%1 · %2%").arg(dialog.report.bandLabel).arg(dialog.report.percent)
                        + ((dialog.report.suggestedRole ?? "") !== "" ? " · " + dialog.report.suggestedRole : "")
                      : qsTr("Not enough profile information for a score")
                variant: "muted"
                font.pixelSize: Theme.fontXs
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }
    HubLabel { text: qsTr("Personal message"); hint: qsTr("Optional. Why you think they'd fit, what they would work on.") }
    TfTextArea {
        id: message
        maxLength: 600
        placeholderText: qsTr("e.g. We need someone for the backend and your Python projects look great.")
        Layout.fillWidth: true
    }
}

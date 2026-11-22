import QtQuick
import QtQuick.Layouts
import TeamForge

// Approve, waitlist or reject an application, with an optional message to the applicant.
HubDialog {
    id: decision

    property var application: ({})
    property string choice: "approve"
    signal decided(string status)

    readonly property string who: application.applicant ? application.applicant.name : qsTr("this applicant")
    title: choice === "approve" ? qsTr("Approve %1?").arg(who) : choice === "waitlist" ? qsTr("Waitlist %1?").arg(who) : qsTr("Reject %1?").arg(who)
    subtitle: application.kind === "team"
              ? (choice === "approve" ? qsTr("They join %1 as a member and get access to the team chat.").arg(application.targetName)
                                      : qsTr("They can apply again later."))
              : (choice === "approve" ? qsTr("They get a place in %1.").arg(application.targetName)
                 : choice === "waitlist" ? qsTr("They keep their application and can be approved when a place opens.")
                                         : qsTr("They are told your decision."))
    primaryText: choice === "approve" ? qsTr("Approve") : choice === "waitlist" ? qsTr("Waitlist") : qsTr("Reject")
    primaryVariant: choice === "reject" ? "danger" : "primary"

    function openFor(app, newChoice) {
        application = app
        choice = newChoice
        response.clear()
        open()
    }

    onAccepted: {
        const problem = Hub.checkText(response.text, 1000, qsTr("Response"))
        if (problem !== "") {
            error = problem
            return
        }
        busy = true
        const path = (application.kind === "team" ? "/team-applications/" : "/event-applications/") + application.id + "/decision"
        const body = application.kind === "team" ? { decision: choice, response: response.text } : { decision: choice, feedback: response.text }
        Hub.post(path, body, (status, data) => {
            busy = false
            if (status === 200) {
                close()
                decision.decided(data.application.status)
            } else {
                error = data.error ?? qsTr("The decision could not be saved.")
            }
        })
    }

    HubLabel { text: qsTr("Message to the applicant"); hint: qsTr("Optional. Explaining a decision helps, especially a rejection.") }
    TfTextArea { id: response; maxLength: 1000; Layout.fillWidth: true }
}

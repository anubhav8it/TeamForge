import QtQuick
import QtQuick.Layouts
import TeamForge

// Send an anonymous message to a team's leaders, an event's organiser or a person who accepts
// anonymous messages. The recipient never sees who sent it (the server keeps that out of every
// answer it gives them); what anonymity means is spelled out next to the form.
HubDialog {
    id: anon

    property string targetKind: "team"
    property string targetId
    property string targetName
    signal sent(string threadId)

    title: qsTr("Message %1 anonymously").arg(targetName)
    subtitle: targetKind === "team" ? qsTr("Goes to the team leader and team admins.")
            : targetKind === "event" ? qsTr("Goes to the event's organiser.") : qsTr("Goes only to this person.")
    primaryText: qsTr("Send anonymously")
    primaryEnabled: message.text.trim() !== "" && !message.tooLong

    function openFor(kind, id, name) {
        targetKind = kind
        targetId = String(id)
        targetName = name
        message.clear()
        open()
    }

    onAccepted: {
        const problem = Hub.checkText(message.text, 1000, qsTr("Message"), true)
        if (problem !== "") {
            error = problem
            return
        }
        busy = true
        Hub.post("/anonymous", { targetKind: targetKind, targetId: targetId, body: message.text }, (status, data) => {
            busy = false
            if (status === 201) {
                close()
                anon.sent(data.thread.id)
            } else if (status === 409 && data.threadId) {
                close()
                anon.sent(data.threadId)
            } else {
                error = data.error ?? qsTr("The message could not be sent.")
            }
        })
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: notice.implicitHeight + 20
        radius: Theme.radiusSmall
        color: Qt.rgba(Theme.violet.r, Theme.violet.g, Theme.violet.b, 0.12)
        border.color: Qt.rgba(Theme.violet.r, Theme.violet.g, Theme.violet.b, 0.4)
        TfText {
            id: notice
            anchors.fill: parent
            anchors.margins: 10
            text: Hub.anonymityNotice
            color: "#DCD3FF"
            font.pixelSize: Theme.fontXs
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }
    }
    HubLabel { text: qsTr("Your message"); hint: qsTr("Ask a question or say what interests you. Up to 1000 characters.") }
    TfTextArea { id: message; maxLength: 1000; minimumHeight: 110; Layout.fillWidth: true }
}

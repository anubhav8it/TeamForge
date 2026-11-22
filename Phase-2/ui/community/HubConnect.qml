import QtQuick
import QtQuick.Layouts
import TeamForge

// Before the Community is reachable. In the browser the page's own sign-in is used, so this only
// shows progress or a problem. The desktop app signs in to a TeamForge server here, with an
// account created on that server's web page.
Item {
    id: connect

    property bool busy: false
    property string error: ""

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(480, parent.width - 48)
        implicitHeight: column.implicitHeight + 48
        height: implicitHeight
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: 24
            spacing: 12

            TfText { text: qsTr("Community"); variant: "eyebrow" }
            TfText {
                text: Hub.browser ? qsTr("Connecting to TeamForge…") : qsTr("Sign in to TeamForge")
                variant: "section"
                Layout.fillWidth: true
            }
            TfText {
                text: Hub.browser
                      ? (Hub.statusText !== "" ? Hub.statusText : qsTr("Loading your teams, messages and applications."))
                      : qsTr("Teams, chat and applications are shared with everyone on a TeamForge server. Create your account on its web page, then sign in here.")
                variant: "secondary"
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
                Layout.fillWidth: true
            }

            TfButton {
                visible: Hub.browser
                text: qsTr("Try again")
                compact: true
                onClicked: Hub.refresh()
            }

            ColumnLayout {
                visible: !Hub.browser
                spacing: 10
                Layout.fillWidth: true
                HubLabel { text: qsTr("Server address") }
                TfTextField {
                    id: server
                    text: Hub.serverUrl !== "" ? Hub.serverUrl : "https://teamforge-geu.onrender.com"
                    placeholderText: qsTr("https://teamforge-geu.onrender.com")
                    Layout.fillWidth: true
                }
                HubLabel { text: qsTr("Email") }
                TfTextField { id: email; placeholderText: qsTr("you@example.com"); Layout.fillWidth: true }
                HubLabel { text: qsTr("Password") }
                TfTextField {
                    id: password
                    echoMode: TextInput.Password
                    placeholderText: qsTr("Password")
                    Layout.fillWidth: true
                    onAccepted: signInButton.clicked()
                }
                TfText {
                    visible: connect.error !== ""
                    text: connect.error
                    color: Theme.accentHover
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                    Layout.fillWidth: true
                }
                TfButton {
                    id: signInButton
                    text: connect.busy ? qsTr("Signing in…") : qsTr("Sign in")
                    variant: "primary"
                    enabled: !connect.busy && email.text.trim() !== "" && password.text !== ""
                    Layout.fillWidth: true
                    onClicked: {
                        connect.busy = true
                        connect.error = ""
                        Hub.signIn(server.text, email.text, password.text, (status, data) => {
                            connect.busy = false
                            if (status === 200)
                                password.text = ""
                            else
                                connect.error = data.error ?? qsTr("Could not sign in.")
                        })
                    }
                }
            }
        }
    }
}

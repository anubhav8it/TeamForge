pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import TeamForge

// The person's Community profile: what team leaders and organisers see with an application, and
// what teammates see in the chat. Separate from the matching profile in "My profile" (which the
// app's matching uses); the first time it suggests details from that profile.
Item {
    id: profile

    signal navigate(string route)
    signal notify(string message)

    readonly property var colors: ["#F0434A", "#5AB0FF", "#3DD598", "#FFB547", "#A78BFA", "#2DD4BF", "#FF8F94"]
    property string avatar: ""
    property string color: ""
    property bool loading: true
    property bool saving: false
    property string error: ""
    property string email: ""
    property bool suggested: false

    function fill(p) {
        name.text = p.name === "New member" ? "" : (p.name ?? "")
        headline.text = p.headline ?? ""
        bio.text = p.bio ?? ""
        skills.text = (p.skills ?? []).join(", ")
        experience.text = p.experience ?? ""
        interests.text = p.interests ?? ""
        anonymous.checked = !!p.acceptsAnonymous
        avatar = p.avatar ?? ""
        color = p.color ?? colors[0]
    }
    Component.onCompleted: Hub.get("/profile", (code, data) => {
        loading = false
        if (code !== 200) return
        email = data.email
        fill(data.profile)
        if (data.needsSetup && data.suggestion) {
            suggested = true
            name.text = data.suggestion.name
            headline.text = data.suggestion.headline
            bio.text = data.suggestion.bio
            skills.text = data.suggestion.skills.join(", ")
            interests.text = data.suggestion.interests
        }
    })

    Connections {
        target: Hub
        function onAvatarPicked(dataUrl) { profile.avatar = dataUrl }
        function onAvatarFailed(message) { profile.notify(message) }
    }
    FileDialog {
        id: fileDialog
        title: qsTr("Choose a profile picture")
        nameFilters: [qsTr("Pictures (*.jpg *.jpeg *.png *.webp *.bmp *.gif)")]
        onAccepted: Hub.avatarFromFile(selectedFile)
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: Math.min(parent.width, 860)
            x: Math.max(0, (parent.width - width) / 2)
            spacing: Theme.gap

            HubSkeleton { visible: profile.loading; rows: 3; Layout.fillWidth: true; Layout.margins: 24 }

            Panel {
                visible: !profile.loading
                title: qsTr("Community profile")
                subtitle: qsTr("Shown to team leaders and organisers with your applications, and to your teammates. Your email (%1) is never shown.").arg(profile.email)
                Layout.fillWidth: true
                Layout.margins: 24

                ColumnLayout {
                    width: parent.width
                    spacing: 12

                    Rectangle {
                        visible: profile.suggested
                        Layout.fillWidth: true
                        implicitHeight: 40
                        radius: Theme.radiusSmall
                        color: Theme.infoSubtle
                        TfText {
                            anchors.fill: parent
                            anchors.margins: 10
                            text: qsTr("Filled in from your TeamForge profile. Check it and save.")
                            color: "#D6EBFF"
                        }
                    }

                    // picture + colour
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        HubAvatar { person: ({ name: name.text, avatar: profile.avatar, color: profile.color }); size: 84 }
                        ColumnLayout {
                            spacing: 8
                            Layout.fillWidth: true
                            RowLayout {
                                spacing: 8
                                TfButton {
                                    text: profile.avatar !== "" ? qsTr("Change picture") : qsTr("Upload picture")
                                    compact: true
                                    onClicked: Hub.browser ? Hub.pickAvatar() : fileDialog.open()
                                }
                                TfButton {
                                    visible: profile.avatar !== ""
                                    text: qsTr("Remove")
                                    compact: true
                                    variant: "ghost"
                                    onClicked: profile.avatar = ""
                                }
                            }
                            TfText { text: qsTr("Without a picture, your initials show on this colour:"); variant: "muted"; font.pixelSize: Theme.fontXs }
                            Row {
                                spacing: 8
                                Repeater {
                                    model: profile.colors
                                    delegate: Rectangle {
                                        id: swatch
                                        required property string modelData
                                        width: 26; height: 26; radius: 13
                                        color: modelData
                                        border.width: profile.color === modelData ? 3 : 0
                                        border.color: "#FFFFFF"
                                        Accessible.role: Accessible.RadioButton
                                        Accessible.name: qsTr("Colour %1").arg(modelData)
                                        Accessible.checked: profile.color === modelData
                                        Accessible.onPressAction: profile.color = swatch.modelData
                                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                                        TapHandler { onTapped: profile.color = swatch.modelData }
                                    }
                                }
                            }
                        }
                    }

                    HubLabel { text: qsTr("Name"); hint: qsTr("Required. How others see you.") }
                    TfTextField { id: name; maximumLength: 60; placeholderText: qsTr("Your name"); Layout.fillWidth: true }
                    HubLabel { text: qsTr("Headline") }
                    TfTextField { id: headline; maximumLength: 120; placeholderText: qsTr("e.g. Frontend developer, B.Tech CSE 2nd year"); Layout.fillWidth: true }
                    HubLabel { text: qsTr("About you") }
                    TfTextArea { id: bio; maxLength: 1500; Layout.fillWidth: true }
                    HubLabel { text: qsTr("Skills"); hint: qsTr("Separate with commas, up to 20") }
                    TfTextField { id: skills; Layout.fillWidth: true }
                    HubLabel { text: qsTr("Experience") }
                    TfTextArea { id: experience; maxLength: 1500; Layout.fillWidth: true }
                    HubLabel { text: qsTr("Interests") }
                    TfTextArea { id: interests; maxLength: 1000; minimumHeight: 56; Layout.fillWidth: true }
                    TfCheck {
                        id: anonymous
                        text: qsTr("Accept anonymous messages (e.g. questions about my work or projects)")
                        onToggled: value => anonymous.checked = value
                    }
                    TfText {
                        visible: profile.error !== ""
                        text: profile.error
                        color: Theme.accentHover
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Item { Layout.fillWidth: true }
                        TfButton { visible: !Hub.browser; text: qsTr("Sign out of the server"); compact: true; variant: "ghost"; onClicked: Hub.signOut() }
                        TfButton {
                            text: profile.saving ? qsTr("Saving…") : qsTr("Save profile")
                            variant: "primary"
                            enabled: !profile.saving && name.text.trim() !== ""
                            onClicked: {
                                const problem = Hub.checkText(bio.text, 1500, qsTr("About you")) || Hub.checkText(experience.text, 1500, qsTr("Experience"))
                                                || Hub.checkText(interests.text, 1000, qsTr("Interests"))
                                if (problem !== "") { profile.error = problem; return }
                                profile.saving = true
                                profile.error = ""
                                Hub.post("/profile", {
                                    name: name.text, headline: headline.text, bio: bio.text, skills: skills.text, experience: experience.text,
                                    interests: interests.text, avatar: profile.avatar, color: profile.color, acceptsAnonymous: anonymous.checked
                                }, (code, data) => {
                                    profile.saving = false
                                    if (code === 200) { profile.suggested = false; profile.notify(qsTr("Profile saved")) }
                                    else profile.error = data.error ?? qsTr("The profile could not be saved.")
                                })
                            }
                        }
                    }
                }
            }
        }
    }
}

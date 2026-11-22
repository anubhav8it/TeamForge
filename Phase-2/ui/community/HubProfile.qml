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
    property var current: ({})        // the saved profile (for the picker's current photo)
    property bool linked: false       // uses a participant profile: name and academics come from it
    property string program: ""

    function fill(p) {
        name.text = p.name === "New member" ? "" : (p.name ?? "")
        headline.text = p.headline ?? ""
        bio.text = p.bio ?? ""
        skills.text = (p.skills ?? []).join(", ")
        experience.text = p.experience ?? ""
        interests.text = p.interests ?? ""
        anonymous.checked = !!p.acceptsAnonymous
        emailUpdates.checked = p.emailUpdates !== false
        openToInvites.checked = p.openToInvites !== false
        const links = p.links ?? []
        link1.text = links[0] ?? ""
        link2.text = links[1] ?? ""
        link3.text = links[2] ?? ""
        avatar = p.avatar ?? ""
        color = p.color ?? colors[0]
        program = p.program ?? ""
        current = p
        linked = (p.studentId ?? "") !== ""
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
                Layout.margins: profile.width < 520 ? 12 : 24

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

                    // photo (the same picker as in onboarding) + colour for the initials
                    PhotoPicker {
                        id: photoPicker
                        Layout.fillWidth: true
                        compact: profile.width >= 460
                        size: 88
                        name: name.text
                        person: ({ id: profile.current.id, name: name.text, avatar: profile.current.avatar, color: profile.color })
                        onCropped: url => profile.avatar = url
                        onRemoved: profile.avatar = ""
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        ColumnLayout {
                            spacing: 8
                            Layout.fillWidth: true
                            TfText { text: qsTr("Without a photo, your initials show on this colour:"); variant: "muted"; font.pixelSize: Theme.fontXs }
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

                    HubLabel {
                        text: qsTr("Name")
                        hint: profile.linked ? qsTr("Comes from your TeamForge profile (My profile → Edit), so it is the same everywhere.")
                                             : qsTr("Required. How others see you.")
                    }
                    TfTextField { id: name; maximumLength: 60; placeholderText: qsTr("Your name"); readOnly: profile.linked; opacity: profile.linked ? 0.75 : 1; Layout.fillWidth: true }
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
                    HubLabel { text: qsTr("Portfolio and GitHub links"); hint: qsTr("Optional, up to 3 full https:// addresses. Team leaders see them when they look at your profile.") }
                    TfTextField { id: link1; placeholderText: "https://github.com/your-name"; maximumLength: 200; Layout.fillWidth: true }
                    TfTextField { id: link2; placeholderText: qsTr("https://… (portfolio, project, LinkedIn)"); maximumLength: 200; Layout.fillWidth: true }
                    TfTextField { id: link3; placeholderText: "https://…"; maximumLength: 200; Layout.fillWidth: true }
                    TfCheck {
                        id: openToInvites
                        Layout.fillWidth: true
                        wrapWidth: width
                        checked: true
                        text: qsTr("Open to team invitations: team leaders can find me in Find members (with my profile and skills) and invite me")
                        onToggled: value => openToInvites.checked = value
                    }
                    TfCheck {
                        id: anonymous
                        Layout.fillWidth: true
                        wrapWidth: width
                        text: qsTr("Accept anonymous messages (e.g. questions about my work or projects)")
                        onToggled: value => anonymous.checked = value
                    }
                    TfCheck {
                        id: emailUpdates
                        Layout.fillWidth: true
                        wrapWidth: width
                        checked: true
                        text: qsTr("Email me about decisions on my applications, invitations and team changes")
                        onToggled: value => emailUpdates.checked = value
                    }
                    TfText {
                        visible: profile.error !== ""
                        text: profile.error
                        color: Theme.accentHover
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    // right-aligned, wrapping on narrow screens
                    Flow {
                        Layout.fillWidth: true
                        layoutDirection: Qt.RightToLeft
                        spacing: 8
                        TfButton {
                            text: profile.saving ? qsTr("Saving…") : qsTr("Save profile")
                            variant: "primary"
                            enabled: !profile.saving && name.text.trim() !== ""
                            onClicked: {
                                const links = [link1.text, link2.text, link3.text].map(t => t.trim()).filter(t => t !== "")
                                const badLink = links.find(l => !/^https:\/\/[A-Za-z0-9.-]+(:\d+)?(\/\S*)?$/.test(l))
                                const problem = Hub.checkText(bio.text, 1500, qsTr("About you")) || Hub.checkText(experience.text, 1500, qsTr("Experience"))
                                                || Hub.checkText(interests.text, 1000, qsTr("Interests"))
                                                || (badLink ? qsTr("Links must be full https:// addresses: %1").arg(badLink) : "")
                                if (problem !== "") { profile.error = problem; return }
                                profile.saving = true
                                profile.error = ""
                                Hub.post("/profile", {
                                    name: name.text, headline: headline.text, bio: bio.text, skills: skills.text, experience: experience.text,
                                    interests: interests.text, avatar: profile.avatar, color: profile.color, acceptsAnonymous: anonymous.checked,
                                    emailUpdates: emailUpdates.checked, program: profile.program, links: links,
                                    openToInvites: openToInvites.checked
                                }, (code, data) => {
                                    profile.saving = false
                                    if (code === 200) {
                                        profile.suggested = false
                                        profile.fill(data.profile)
                                        photoPicker.pending = ""
                                        photoPicker.removedPending = false
                                        profile.notify(qsTr("Profile saved"))
                                    }
                                    else profile.error = data.error ?? qsTr("The profile could not be saved.")
                                })
                            }
                        }
                        TfButton { visible: !Hub.browser; text: qsTr("Sign out of the server"); compact: true; variant: "ghost"; onClicked: Hub.signOut() }
                    }
                }
            }
        }
    }
}

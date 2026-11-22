pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import TeamForge

// Profile photo: a large preview (the person's current photo or initials), Upload / Change /
// Remove, and cropping before the photo is used. The owner decides when to save: `cropped`
// gives the new picture (a small JPEG data: URL), `removed` asks to drop it. On the browser the
// phone's or computer's own file picker opens; the desktop app uses a file dialog. Photos are
// kept by the TeamForge server, so the desktop app needs to be signed in to it.
ColumnLayout {
    id: picker

    property string name                // for the initials
    property string seed                // participant id (finds the current photo)
    property var person: null           // or a Community profile
    property string pending: ""         // a picked, not yet saved photo (shown instead)
    property bool removedPending: false // removal asked, not yet saved
    property int size: 104
    property bool compact: false        // buttons beside the photo instead of below
    readonly property bool available: Hub.connected
    readonly property bool hasPhoto: pending !== "" || (!removedPending && currentRef !== "")
    readonly property string currentRef: {
        void Hub.identityRevision
        if (person && person.avatar) return person.avatar
        const me = seed !== "" ? Hub.identityForStudent(seed) : null
        return me && me.avatar ? me.avatar : ""
    }
    property string error: ""
    property bool awaiting: false
    signal cropped(string dataUrl)
    signal removed()

    function choose() {
        error = ""
        awaiting = true
        if (Hub.browser)
            Hub.pickAvatar()
        else
            fileDialog.open()
    }

    spacing: 10

    Connections {
        target: Hub
        enabled: picker.awaiting
        function onAvatarSourceReady(preview, width, height) {
            picker.awaiting = false
            crop.show(preview, width, height)
        }
        function onAvatarFailed(message) {
            picker.awaiting = false
            picker.error = message
        }
    }
    FileDialog {
        id: fileDialog
        title: qsTr("Choose a profile photo")
        nameFilters: [qsTr("Pictures (*.jpg *.jpeg *.png *.webp *.bmp *.gif *.heic)")]
        onAccepted: Hub.avatarFromFile(selectedFile)
        onRejected: picker.awaiting = false
    }
    PhotoCropDialog {
        id: crop
        onCropped: url => {
            picker.pending = url
            picker.removedPending = false
            picker.cropped(url)
        }
    }

    GridLayout {
        columns: picker.compact ? 2 : 1
        columnSpacing: 16
        rowSpacing: 10
        Layout.fillWidth: true

        // the photo, with a camera badge
        Item {
            implicitWidth: picker.size
            implicitHeight: picker.size
            Layout.alignment: picker.compact ? Qt.AlignVCenter : Qt.AlignHCenter
            // soft halo
            Rectangle {
                anchors.fill: parent
                anchors.margins: -6
                radius: width / 2
                color: "transparent"
                border.width: 2
                border.color: picker.hasPhoto ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.45) : Theme.borderStrong
            }
            Avatar {
                anchors.fill: parent
                size: picker.size
                name: picker.name
                seed: picker.removedPending || picker.pending !== "" ? "" : picker.seed
                person: picker.removedPending || picker.pending !== "" ? null : picker.person
                photo: picker.pending
            }
            Rectangle {
                visible: picker.available
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: Math.round(picker.size * 0.32)
                height: width
                radius: width / 2
                color: Theme.accent
                border.width: 3
                border.color: Theme.background
                // camera: body and lens
                Rectangle { anchors.centerIn: parent; width: parent.width * 0.5; height: parent.width * 0.36; radius: 3; color: "transparent"; border.width: 2; border.color: "#FFFFFF" }
                Rectangle { anchors.centerIn: parent; width: parent.width * 0.18; height: width; radius: width / 2; color: "#FFFFFF" }
                Accessible.role: Accessible.Button
                Accessible.name: picker.hasPhoto ? qsTr("Change photo") : qsTr("Upload photo")
                Accessible.onPressAction: picker.choose()
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: picker.choose() }
            }
        }

        ColumnLayout {
            spacing: 6
            Layout.fillWidth: true
            Layout.alignment: picker.compact ? Qt.AlignVCenter : Qt.AlignHCenter
            TfText {
                visible: picker.compact
                text: qsTr("Profile photo")
                font.weight: Font.Bold
            }
            TfText {
                visible: picker.compact || !picker.available
                text: picker.available ? qsTr("Shown with your name everywhere in TeamForge. JPEG or PNG; you can crop it.")
                                       : qsTr("Sign in to the TeamForge server (Community) to add a photo.")
                variant: "muted"
                font.pixelSize: Theme.fontXs
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
                Layout.fillWidth: true
            }
            Flow {
                visible: picker.available
                spacing: 8
                Layout.alignment: picker.compact ? Qt.AlignLeft : Qt.AlignHCenter
                TfButton {
                    text: picker.awaiting ? qsTr("Opening…") : picker.hasPhoto ? qsTr("Change photo") : qsTr("Upload photo")
                    compact: true
                    enabled: !picker.awaiting
                    onClicked: picker.choose()
                }
                TfButton {
                    visible: picker.hasPhoto
                    text: qsTr("Remove")
                    compact: true
                    variant: "ghost"
                    onClicked: {
                        picker.pending = ""
                        picker.removedPending = true
                        picker.removed()
                    }
                }
            }
            TfText {
                visible: picker.error !== ""
                text: picker.error
                color: Theme.warning
                font.pixelSize: Theme.fontXs
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
                Layout.fillWidth: true
            }
        }
    }
}

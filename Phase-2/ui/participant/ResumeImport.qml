pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Shapes
import TeamForge

// "Start from your resume": drop a resume here or choose one (a document, or a photo or scan of
// it); the backend reads it in the background (Backend.resume) and the review drawer opens when
// it is ready. A photo or scan is only sent to be read (Azure, through the server) after the
// person agrees on this card ("confirm"). Nothing is saved until the person applies what they
// reviewed, so this card only starts and reports the work.
Rectangle {
    id: card

    signal reviewRequested()

    readonly property var resume: Backend.resume
    readonly property string phase: resume.state ?? "idle"
    readonly property bool web: Qt.platform.os === "wasm"
    readonly property bool readsPictures: resume.readsPictures ?? false
    readonly property bool scan: (resume.picture?.kind ?? "") === "scan"

    function choose() {
        if (web)
            Backend.chooseResumeFile()
        else
            fileDialog.open()
    }

    implicitHeight: body.implicitHeight + 36
    radius: Theme.radius
    // A drop zone: tinted blue, outlined with a dashed line that brightens while a file is
    // dragged over it.
    border.width: 0
    gradient: Gradient {
        GradientStop { position: 0.0; color: Qt.rgba(Theme.info.r, Theme.info.g, Theme.info.b, drop.containsDrag ? 0.20 : 0.09) }
        GradientStop { position: 1.0; color: Qt.rgba(Theme.info.r, Theme.info.g, Theme.info.b, drop.containsDrag ? 0.10 : 0.03) }
    }
    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: drop.containsDrag ? Theme.info : Qt.rgba(Theme.info.r, Theme.info.g, Theme.info.b, 0.42)
            strokeWidth: drop.containsDrag ? 2 : 1.5
            strokeStyle: ShapePath.DashLine
            dashPattern: [5, 4]
            fillColor: "transparent"
            PathRectangle { x: 1; y: 1; width: card.width - 2; height: card.height - 2; radius: Theme.radius - 1 }
        }
    }

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Import from your resume")

    FileDialog {
        id: fileDialog
        title: qsTr("Choose your resume")
        nameFilters: [qsTr("Resumes (*.pdf *.docx *.odt *.rtf *.txt *.md *.html *.htm *.jpg *.jpeg *.png *.bmp *.gif *.webp *.tif *.tiff)"),
                      qsTr("All files (*)")]
        onAccepted: Backend.analyseResumeFile(selectedFile)
    }

    DropArea {
        id: drop
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: event => {
            if (event.hasUrls && event.urls.length > 0) {
                Backend.analyseResumeFile(event.urls[0])
                event.accept()
            }
        }
    }

    // Open the review as soon as an analysis finishes.
    onPhaseChanged: if (phase === "ready") card.reviewRequested()

    RowLayout {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: 18
        spacing: 16

        Rectangle {
            id: badge
            Layout.alignment: Qt.AlignTop
            implicitWidth: 56
            implicitHeight: 56
            radius: 16
            readonly property color tone: card.phase === "error" ? Theme.warning : card.phase === "ready" ? Theme.success : Theme.info
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(badge.tone.r, badge.tone.g, badge.tone.b, 0.34) }
                GradientStop { position: 1.0; color: Qt.rgba(badge.tone.r, badge.tone.g, badge.tone.b, 0.12) }
            }
            border.color: Qt.rgba(badge.tone.r, badge.tone.g, badge.tone.b, 0.5)
            Rectangle { x: 12; y: 1; width: parent.width - 24; height: 1; color: "#33FFFFFF" }

            // Upload: an arrow rising out of a tray (bobs gently while the file is read).
            Shape {
                id: uploadIcon
                visible: card.phase === "idle" || card.phase === "working"
                anchors.centerIn: parent
                width: 24
                height: 24
                preferredRendererType: Shape.CurveRenderer
                property real lift: 0
                transform: Translate { y: -uploadIcon.lift }
                SequentialAnimation on lift {
                    running: card.phase === "working"
                    loops: Animation.Infinite
                    alwaysRunToEnd: true
                    NumberAnimation { from: 0; to: 3; duration: 450; easing.type: Easing.InOutQuad }
                    NumberAnimation { from: 3; to: 0; duration: 450; easing.type: Easing.InOutQuad }
                }
                ShapePath {
                    strokeColor: Qt.lighter(Theme.info, 1.2)
                    strokeWidth: 2.2
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    startX: 12; startY: 16
                    PathLine { x: 12; y: 3 }
                    PathMove { x: 6.5; y: 8.5 }
                    PathLine { x: 12; y: 3 }
                    PathLine { x: 17.5; y: 8.5 }
                    PathMove { x: 3; y: 15 }
                    PathLine { x: 3; y: 20 }
                    PathLine { x: 21; y: 20 }
                    PathLine { x: 21; y: 15 }
                }
            }
            Text {
                visible: !uploadIcon.visible
                anchors.centerIn: parent
                text: card.phase === "ready" ? "✓" : card.phase === "error" ? "!" : "?"
                font.family: Theme.fontFamily
                font.pixelSize: 24
                font.weight: Font.ExtraBold
                color: card.phase === "error" ? Theme.warning : card.phase === "ready" ? Theme.success : Theme.info
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            TfText {
                text: card.phase === "working" ? qsTr("Reading %1").arg(card.resume.fileName ?? "")
                    : card.phase === "ready" ? qsTr("Resume read: %1").arg(card.resume.fileName ?? "")
                    : card.phase === "error" ? qsTr("That file couldn't be used")
                    : card.phase === "confirm" ? (card.scan ? qsTr("Read the text from this scan?") : qsTr("Read the text from this photo?"))
                    : qsTr("Start from your resume")
                font.pixelSize: Theme.fontMd
                font.weight: Font.ExtraBold
                Layout.fillWidth: true
            }
            TfText {
                visible: card.phase !== "working"
                text: card.phase === "error" ? (card.resume.error ?? "")
                    : card.phase === "ready" ? qsTr("Check what was found, correct it and choose what to add.")
                    : card.phase === "confirm"
                      ? (card.resume.reason ?? "") + " "
                        + qsTr("To read it, TeamForge sends %1 (%2) to Microsoft Azure AI Document Intelligence. "
                               + "Azure returns the text and its copy is deleted straight away; TeamForge doesn't keep the picture.")
                              .arg(card.resume.fileName ?? "").arg(card.resume.picture?.sizeText ?? "")
                    : (card.readsPictures
                         ? qsTr("Choose your resume: a PDF, Word, ODT, RTF, text or HTML file, or a clear photo or scan of it (up to %1 MB).")
                         : card.web ? qsTr("Choose your resume: a PDF, Word, ODT, RTF, text or HTML file (up to %1 MB).")
                         : qsTr("Drop your resume here or choose it: a PDF, Word, ODT, RTF, text or HTML file (up to %1 MB). "
                                + "Photos and scans are read in the web version."))
                          .arg(Backend.resumeMaxMegabytes)
                      + " " + qsTr("Nothing is added until you review it.")
                variant: card.phase === "error" ? "body" : "secondary"
                color: card.phase === "error" ? Theme.warning : Theme.textSecondary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            // Progress while the file is read in the background.
            ColumnLayout {
                visible: card.phase === "working"
                Layout.fillWidth: true
                spacing: 6
                MeterBar {
                    Layout.fillWidth: true
                    value: card.resume.progress ?? 0
                    fillColor: Theme.info
                }
                TfText {
                    text: card.resume.stage ?? ""
                    variant: "muted"
                    font.weight: Font.Bold
                }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: 8
            TfButton {
                visible: card.phase === "ready"
                text: qsTr("Review")
                variant: "primary"
                compact: true
                onClicked: card.reviewRequested()
            }
            TfButton {
                visible: card.phase === "confirm"
                text: qsTr("Read the text")
                variant: "primary"
                compact: true
                Accessible.description: qsTr("Sends the picture to Azure to read its text")
                onClicked: Backend.confirmResumeReading()
            }
            TfButton {
                visible: card.phase === "working" || card.phase === "confirm"
                text: qsTr("Cancel")
                variant: "ghost"
                compact: true
                onClicked: Backend.clearResume()
            }
            TfButton {
                visible: card.phase !== "working" && card.phase !== "confirm"
                text: card.phase === "idle" ? qsTr("Choose file") : qsTr("Choose another")
                variant: card.phase === "idle" ? "primary" : "secondary"
                compact: true
                onClicked: card.choose()
            }
        }
    }
}

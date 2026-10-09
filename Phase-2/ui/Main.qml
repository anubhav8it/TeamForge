pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Application shell. Starts on the workspace choice (EntryScreen); each of the three
// workspaces (participant, host, developer) brings its own pages, loaded only while active.
// The shell owns the sidebar, top bar, error banner, toast and the Match insights drawer.
// All data, permissions and actions go through the C++ Backend singleton.
ApplicationWindow {
    id: window

    width: Math.min(1440, Screen.desktopAvailableWidth - 40)
    height: Math.min(900, Screen.desktopAvailableHeight - 40)
    minimumWidth: 1100
    minimumHeight: 660
    visible: true
    title: qsTr("TeamForge") + (Backend.workspace !== "" ? " — " + Theme.workspaceName(Backend.workspace) : "")
    color: Theme.background
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontBase

    palette {
        window: Theme.surface
        windowText: Theme.text
        base: Theme.input
        text: Theme.text
        button: Theme.surfaceRaised
        buttonText: Theme.text
        highlight: Theme.accent
        highlightedText: "#FFFFFF"
        mid: Theme.borderStrong
        dark: Theme.border
        light: Theme.surfaceHover
        placeholderText: Theme.textMuted
        toolTipBase: Theme.surfaceRaised
        toolTipText: Theme.text
    }

    // In the browser build the page gives the app its own area (a QScreen); fill it without
    // window decorations. The desktop window is unchanged.
    Component.onCompleted: {
        if (Qt.platform.os === "wasm")
            window.showFullScreen()
    }

    property int currentPage: 0
    // Icon-only sidebar on smaller windows (e.g. 1280 x 720) so the pages keep their room.
    readonly property bool compact: width < 1360
    readonly property string workspace: Backend.workspace

    readonly property var navigation: ({
        participant: [
            { title: qsTr("Home"), nav: qsTr("Your workspace"), glyph: "⌂", subtitle: qsTr("Who you are, what you're good at and where you fit") },
            { title: qsTr("My profile"), nav: qsTr("Skills and experience"), glyph: "◉", subtitle: qsTr("How hosts see you when they build teams") },
            { title: qsTr("Opportunities"), nav: qsTr("Projects that fit you"), glyph: "✦", subtitle: qsTr("Projects ranked by how well you fit them") }
        ],
        host: [
            { title: qsTr("Overview"), nav: qsTr("Home"), subtitle: qsTr("Your active project at a glance") },
            { title: qsTr("Projects"), nav: qsTr("What the team needs"), subtitle: qsTr("Set the skills and team size a project needs") },
            { title: qsTr("Applicants"), nav: qsTr("Review interest"), subtitle: qsTr("Participants who want to join your project") },
            { title: qsTr("Participants"), nav: qsTr("Skill profiles"), subtitle: qsTr("%1 profiles with skills and experience").arg(Backend.students.length) },
            { title: qsTr("Matching"), nav: qsTr("Best matches & team"), subtitle: qsTr("Pick people, check coverage, save the team") }
        ],
        developer: [
            { title: qsTr("Overview"), nav: qsTr("The data at a glance"), glyph: "▣", subtitle: qsTr("TeamForge %1 · everything in this local workspace").arg(Backend.version) },
            { title: qsTr("Participants"), nav: qsTr("Every record"), glyph: "◉", subtitle: qsTr("The complete participant database: inspect, edit, save") },
            { title: qsTr("Projects"), nav: qsTr("Requirements and teams"), glyph: "✦", subtitle: qsTr("Every project and saved team, with raw records") },
            { title: qsTr("Skills"), nav: qsTr("The skill universe"), glyph: "◆", subtitle: qsTr("Every skill, its category, usage and structures") },
            { title: qsTr("Workspace"), nav: qsTr("Previews and data"), glyph: "⚙", subtitle: qsTr("Preview the product, manage local data, system tools") }
        ]
    })
    readonly property var pages: navigation[workspace] ?? []
    // A participant without a profile (or editing it) sees the profile editor, whatever the page.
    property bool participantEditing: false
    readonly property bool profileEditorShown: workspace === "participant"
                                               && (Backend.myProfileId === "" || participantEditing)
    readonly property var page: profileEditorShown
                                ? { title: Backend.myProfileId === "" ? qsTr("Welcome to TeamForge") : qsTr("Edit profile"),
                                    subtitle: qsTr("Four short steps; you can change everything later") }
                                : pages[Math.min(currentPage, pages.length - 1)] ?? ({})

    // A new workspace starts on its first page with nothing from the previous one left open.
    onWorkspaceChanged: {
        currentPage = 0
        insights.close()
    }

    function go(index) {
        currentPage = index
    }
    // Why this match? for a host candidate (scored against the current team).
    function explain(studentId) {
        insights.showCandidate(studentId)
    }
    // Why this match? for a participant's fit with a project.
    function explainFit(studentId, requirementId) {
        insights.showFit(studentId, requirementId)
    }

    EntryScreen {
        anchors.fill: parent
        visible: window.workspace === ""
        onChosen: workspace => Backend.enterWorkspace(workspace)
    }

    RowLayout {
        anchors.fill: parent
        visible: window.workspace !== ""
        spacing: 0

        Sidebar {
            pages: window.pages
            currentIndex: window.currentPage
            compact: window.compact
            navigationEnabled: window.workspace !== "participant" || Backend.myProfileId !== ""
            Layout.fillHeight: true
            Layout.preferredWidth: window.compact ? 72 : 252
            Behavior on Layout.preferredWidth { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
            onNavigate: index => window.go(index)
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Developer previewing another workspace: small, but always visible.
            Rectangle {
                visible: Backend.previewing
                Layout.fillWidth: true
                implicitHeight: 46
                color: Qt.rgba(Theme.violet.r, Theme.violet.g, Theme.violet.b, 0.14)
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Qt.rgba(Theme.violet.r, Theme.violet.g, Theme.violet.b, 0.4) }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 12
                    spacing: 12
                    Chip {
                        text: qsTr("DEVELOPER PREVIEW")
                        tone: "tint"
                        tint: Qt.lighter(Theme.violet, 1.2)
                    }
                    TfText {
                        text: qsTr("%1 workspace, with its permissions").arg(Theme.workspaceName(window.workspace))
                        font.weight: Font.Bold
                        color: Qt.lighter(Theme.violet, 1.3)
                        Layout.fillWidth: true
                    }
                    Segmented {
                        options: [{ text: qsTr("Participant"), value: "participant" }, { text: qsTr("Host"), value: "host" }]
                        currentValue: window.workspace
                        onActivated: value => Backend.previewWorkspace(value)
                    }
                    TfButton {
                        text: qsTr("Back to Developer")
                        compact: true
                        onClicked: Backend.endPreview()
                    }
                }
            }

            TopBar {
                title: window.page.title ?? ""
                subtitle: window.page.subtitle ?? ""
                showProjectSwitcher: window.workspace === "host" && window.currentPage !== 1
                Layout.fillWidth: true
            }

            ErrorBanner {
                message: Backend.lastError
                Layout.fillWidth: true
                Layout.topMargin: shown ? 12 : 0
                onDismissed: Backend.clearError()
            }

            Loader {
                id: workspaceLoader
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                sourceComponent: window.workspace === "participant" ? participantWorkspace
                               : window.workspace === "host" ? hostWorkspace
                               : window.workspace === "developer" ? developerWorkspace : null
            }
        }
    }

    Component {
        id: participantWorkspace
        ParticipantWorkspace {
            currentPage: window.currentPage
            onEditingChanged: window.participantEditing = editing
            Component.onDestruction: window.participantEditing = false
            onNavigate: index => window.go(index)
            onExplainFit: (studentId, requirementId) => window.explainFit(studentId, requirementId)
            onNotify: message => toast.show(message)
        }
    }
    Component {
        id: hostWorkspace
        HostWorkspace {
            currentPage: window.currentPage
            onNavigate: index => window.go(index)
            onExplain: studentId => window.explain(studentId)
            onExplainFit: (studentId, requirementId) => window.explainFit(studentId, requirementId)
            onNotify: message => toast.show(message)
            explainedId: insights.opened ? insights.studentId : ""
        }
    }
    Component {
        id: developerWorkspace
        DeveloperWorkspace {
            currentPage: window.currentPage
            onNavigate: index => window.go(index)
            onExplainFit: (studentId, requirementId) => window.explainFit(studentId, requirementId)
            onNotify: message => toast.show(message)
        }
    }

    MatchInsightsDrawer {
        id: insights
        onNotify: message => toast.show(message)
    }

    // An error replaces any pending confirmation so the two never overlap.
    Connections {
        target: Backend
        function onLastErrorChanged() { if (Backend.lastError !== "") toast.hide() }
    }

    Toast {
        id: toast
        // In the overlay layer so it also shows above the drawers; bottom centre.
        parent: Overlay.overlay
        x: Math.round((parent.width - width) / 2)
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 28
        z: 1000
    }
}

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
    // Phones (and narrow browser windows): no sidebar; a top bar with a navigation drawer, and
    // bottom tabs in the Participant workspace. Pages lay themselves out for the width.
    readonly property bool phone: width < 760
    readonly property string workspace: Backend.workspace

    readonly property var navigation: ({
        participant: [
            { title: qsTr("Home"), short: qsTr("Home"), nav: qsTr("Your workspace"), glyph: "⌂", subtitle: qsTr("Who you are, what you're good at and where you fit") },
            { title: qsTr("My profile"), short: qsTr("Profile"), nav: qsTr("Skills and experience"), glyph: "◉", subtitle: qsTr("How hosts see you when they build teams") },
            { title: qsTr("Opportunities"), short: qsTr("Projects"), nav: qsTr("Projects that fit you"), glyph: "✦", subtitle: qsTr("Projects ranked by how well you fit them") },
            { key: "community", title: qsTr("Community"), glyph: "✉",
              nav: Hub.connected ? qsTr("%1 unread · %2 to review").arg(Hub.unreadMessages).arg(Hub.pendingDecisions) : qsTr("Teams, chat, applications"),
              subtitle: qsTr("Teams and their chat, applications, anonymous requests and notifications") }
        ],
        host: [
            { title: qsTr("Overview"), nav: qsTr("Home"), subtitle: qsTr("Your active project at a glance") },
            { title: qsTr("Projects"), nav: qsTr("What the team needs"), subtitle: qsTr("Set the skills and team size a project needs") },
            { title: qsTr("Applicants"), nav: qsTr("Review interest"), subtitle: qsTr("Participants who want to join your project") },
            { title: qsTr("Participants"), nav: qsTr("Skill profiles"), subtitle: qsTr("%1 profiles with skills and experience").arg(Backend.students.length) },
            { title: qsTr("Matching"), nav: qsTr("Best matches & team"), subtitle: qsTr("Pick people, check coverage, save the team") },
            { key: "community", title: qsTr("Community"), glyph: "✉",
              nav: Hub.connected ? qsTr("%1 unread · %2 to review").arg(Hub.unreadMessages).arg(Hub.pendingDecisions) : qsTr("Teams, chat, applications"),
              subtitle: qsTr("Teams and their chat, applications, anonymous requests and notifications") }
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
    // Opens the Community page of the current workspace at `route` ("notifications", "teams/3" ...).
    function openCommunity(route) {
        const index = pages.findIndex(p => p.key === "community")
        if (index < 0)
            return
        Hub.route = route
        go(index)
    }
    // Someone came through a team invitation link (the server remembered it when they signed in):
    // open it in the Community once, as soon as they have a workspace and (participants) a profile.
    property bool linkClaimOpened: false
    function maybeOpenLinkClaim() {
        if (linkClaimOpened || !Hub.summary.linkClaim || workspace === "" || profileEditorShown)
            return
        if (pages.findIndex(p => p.key === "community") < 0)
            return
        linkClaimOpened = true
        openCommunity("join")
    }
    Connections {
        target: Hub
        function onSummaryChanged() { window.maybeOpenLinkClaim() }
    }
    Connections {
        target: Backend
        function onProfileChanged() { Qt.callLater(window.maybeOpenLinkClaim) }
        function onWorkspaceChanged() { Qt.callLater(window.maybeOpenLinkClaim) }
    }

    // Why this match? for a host candidate (scored against the current team).
    function explain(studentId) {
        insights.showCandidate(studentId)
    }
    // Why this match? for a participant's fit with a project.
    function explainFit(studentId, requirementId) {
        insights.showFit(studentId, requirementId)
    }

    // Desktop layouts keep at least 1100 x 660 (a smaller window scrolls); phones get their own.
    Flickable {
        anchors.fill: parent
        contentWidth: appArea.width
        contentHeight: appArea.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: !window.phone && (window.width < 1100 || window.height < 660)

        Item {
            id: appArea
            width: window.phone ? window.width : Math.max(window.width, 1100)
            height: window.phone ? window.height : Math.max(window.height, 660)

            EntryScreen {
                anchors.fill: parent
                visible: window.workspace === ""
                onChosen: workspace => Backend.enterWorkspace(workspace)
            }

            // Midnight page with soft light in the workspace's colour, behind every workspace page.
            AmbientBackground {
                anchors.fill: parent
                visible: window.workspace !== ""
                tone: Theme.workspaceColor(window.workspace)
            }

            RowLayout {
                anchors.fill: parent
                visible: window.workspace !== ""
                spacing: 0

                Sidebar {
                    visible: !window.phone
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

                    MobileBar {
                        visible: window.phone
                        title: window.page.title ?? ""
                        showBell: (window.pages.findIndex(p => p.key === "community") >= 0) && Hub.connected
                        Layout.fillWidth: true
                        onMenuClicked: navDrawer.open()
                        onBellClicked: window.openCommunity("notifications")
                    }

                    TopBar {
                        visible: !window.phone
                        title: window.page.title ?? ""
                        subtitle: window.page.subtitle ?? ""
                        showProjectSwitcher: window.workspace === "host" && window.currentPage !== 1 && (window.page.key ?? "") !== "community"
                        showBell: (window.pages.findIndex(p => p.key === "community") >= 0) && Hub.connected
                        Layout.fillWidth: true
                        onBellClicked: window.openCommunity("notifications")
                    }

                    ErrorBanner {
                        message: Backend.lastError
                        Layout.fillWidth: true
                        Layout.topMargin: shown ? 12 : 0
                        onDismissed: Backend.clearError()
                    }

                    // The Host and Developer workspaces are desktop tools (tables, team building):
                    // on a phone they keep a desktop width and scroll sideways, with a note.
                    readonly property bool desktopTool: window.phone && window.workspace !== "participant"
                                                        && (window.page.key ?? "") !== "community"
                    Rectangle {
                        visible: parent.desktopTool
                        Layout.fillWidth: true
                        implicitHeight: toolNote.implicitHeight + 14
                        color: Theme.warningSubtle
                        TfText {
                            id: toolNote
                            anchors.fill: parent
                            anchors.margins: 7
                            anchors.leftMargin: 14
                            text: qsTr("This workspace is made for larger screens: swipe sideways to see all of it.")
                            color: "#FFE5BD"
                            font.pixelSize: Theme.fontXs
                            wrapMode: Text.WordWrap
                            elide: Text.ElideNone
                        }
                    }
                    Flickable {
                        id: workspaceArea
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        contentWidth: parent.desktopTool ? Math.max(width, 1040) : width
                        contentHeight: height
                        flickableDirection: Flickable.HorizontalFlick
                        boundsBehavior: Flickable.StopAtBounds
                        interactive: contentWidth > width
                        Loader {
                            id: workspaceLoader
                            width: workspaceArea.contentWidth
                            height: workspaceArea.height
                            clip: true
                            sourceComponent: window.workspace === "participant" ? participantWorkspace
                                           : window.workspace === "host" ? hostWorkspace
                                           : window.workspace === "developer" ? developerWorkspace : null
                        }
                    }

                    MobileTabBar {
                        visible: window.phone && window.workspace === "participant" && Backend.myProfileId !== ""
                                 && !window.participantEditing
                        pages: window.pages
                        currentIndex: window.currentPage
                        tone: Theme.workspaceColor(window.workspace)
                        Layout.fillWidth: true
                        onNavigate: index => window.go(index)
                    }
                }
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

    // Phones: the sidebar, as a drawer from the left (the top bar's menu, or a swipe).
    Drawer {
        id: navDrawer
        edge: Qt.LeftEdge
        width: Math.min(300, window.width * 0.86)
        height: window.height
        interactive: window.phone && window.workspace !== ""
        background: Rectangle { color: Theme.sidebar }
        Overlay.modal: Rectangle { color: "#99050810" }
        Sidebar {
            anchors.fill: parent
            pages: window.pages
            currentIndex: window.currentPage
            compact: false
            navigationEnabled: window.workspace !== "participant" || Backend.myProfileId !== ""
            onNavigate: index => {
                window.go(index)
                navDrawer.close()
            }
        }
        Connections {
            target: Backend
            function onWorkspaceChanged() { navDrawer.close() }
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

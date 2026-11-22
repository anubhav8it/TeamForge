pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// One event: description, requirements, eligibility, positions, places and deadline; Apply,
// Withdraw and Message anonymously for participants; for its organiser the settings, a capacity
// overview and the applications by status.
Item {
    id: detail

    property string eventId
    property string tab: ""
    property bool applyRequested: false
    signal navigate(string route)
    signal notify(string message)

    property var event: null
    property bool loading: true
    property string error: ""
    readonly property bool manage: event ? event.canManage : false
    readonly property string currentTab: tab === "applications" && manage ? "applications" : "overview"
    readonly property bool deadlinePassed: event && event.deadline ? event.deadline < Hub.now() : false

    property var apps: ({ items: [], page: 1, pages: 1, total: 0, counts: ({}) })
    property string appStatus: "pending"
    property bool appsLoading: false

    function load() {
        loading = true
        Hub.get("/events/" + encodeURIComponent(eventId), (status, data) => {
            loading = false
            if (status === 200) {
                event = data.event
                if (applyRequested && event.canApply)
                    applyDialog.openFor("event", event.id, event.name, event.positions)
                if (currentTab === "applications")
                    loadApps(1)
            } else {
                error = data.error ?? qsTr("This event could not be loaded.")
            }
        })
    }
    function loadApps(page) {
        appsLoading = true
        Hub.get("/events/" + encodeURIComponent(eventId) + "/applications?page=" + page + (appStatus !== "" ? "&status=" + appStatus : ""),
                (status, data) => {
            appsLoading = false
            if (status === 200)
                apps = data
        })
    }

    onEventIdChanged: load()
    onCurrentTabChanged: if (currentTab === "applications") loadApps(1)
    Component.onCompleted: load()

    ApplyDialog {
        id: applyDialog
        onApplied: status => {
            detail.notify(status === "waitlisted" ? qsTr("The event is full: you're on the waitlist.")
                                                  : qsTr("Application sent. You'll be notified of the decision."))
            detail.load()
        }
    }
    AnonymousDialog {
        id: anonDialog
        onSent: threadId => { detail.notify(qsTr("Sent anonymously. Replies appear under Anonymous › Sent.")); detail.navigate("anonymous/" + threadId) }
    }
    EventSettingsDialog { id: settingsDialog; onSaved: ev => { detail.event = ev; detail.notify(qsTr("Event settings saved")) } }
    DecisionDialog {
        id: decisionDialog
        onDecided: status => { detail.notify(qsTr("Application %1").arg(Hub.statusLabel(status).toLowerCase())); detail.load(); detail.loadApps(detail.apps.page) }
    }
    PersonDialog { id: personDialog }
    ConfirmDialog {
        id: confirm
        property var pendingAction: null
        onConfirmed: if (pendingAction) pendingAction()
    }

    HubSkeleton { anchors.fill: parent; anchors.margins: 24; visible: detail.loading && !detail.event; rows: 3 }
    EmptyState {
        anchors.centerIn: parent
        width: 420
        visible: !detail.loading && !detail.event
        title: qsTr("Event not found")
        message: detail.error
        actionText: qsTr("All events")
        onActionTriggered: detail.navigate("events")
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        visible: !!detail.event

        ColumnLayout {
            width: parent.width
            spacing: Theme.gap

            // ------------------------------------------------------------ header
            Surface {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 18
                implicitHeight: header.implicitHeight + 40
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.border
                ColumnLayout {
                    id: header
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 20
                    spacing: 10
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Chip { visible: detail.event && detail.event.type !== ""; text: detail.event ? detail.event.type : ""; tone: "tint"; tint: Theme.typeColor(detail.event ? detail.event.type : "") }
                        Chip {
                            text: !detail.event ? "" : detail.event.open ? (detail.event.full ? qsTr("Full · waitlist") : qsTr("Open for applications")) : detail.event.closedReason
                            tone: detail.event && detail.event.open ? (detail.event.full ? "warning" : "success") : "muted"
                        }
                        Item { Layout.fillWidth: true }
                        HubStatus { visible: detail.event && !!detail.event.myApplication; status: detail.event && detail.event.myApplication ? detail.event.myApplication.status : "" }
                    }
                    TfText { text: detail.event ? detail.event.name : ""; variant: "title"; font.pixelSize: 26; Layout.fillWidth: true }
                    TfText {
                        text: detail.event ? detail.event.summary : ""
                        variant: "secondary"
                        wrapMode: Text.WordWrap
                        elide: Text.ElideNone
                        Layout.fillWidth: true
                    }
                    TfText {
                        text: !detail.event ? "" : [detail.event.organizer ? qsTr("Organised by %1").arg(detail.event.organizer.name) : "",
                                                   detail.event.deadline ? qsTr("Deadline %1 (%2)").arg(Hub.dateText(detail.event.deadline)).arg(Hub.deadlineText(detail.event.deadline)) : "",
                                                   detail.event.minTeamSize ? qsTr("teams of %1–%2").arg(detail.event.minTeamSize).arg(detail.event.maxTeamSize) : ""]
                                                  .filter(t => t !== "").join(" · ")
                        variant: "muted"
                        font.pixelSize: Theme.fontXs
                        Layout.fillWidth: true
                    }
                    // capacity meter
                    ColumnLayout {
                        visible: detail.event && detail.event.capacity !== null && detail.event.capacity !== undefined
                        Layout.fillWidth: true
                        spacing: 4
                        MeterBar {
                            Layout.fillWidth: true
                            value: detail.event && detail.event.capacity ? Math.min(1, detail.event.approvedCount / detail.event.capacity) : 0
                        }
                        TfText {
                            text: detail.event ? qsTr("%1 of %2 places taken · %3 left · %4 on the waitlist")
                                                 .arg(detail.event.approvedCount).arg(detail.event.capacity).arg(detail.event.spotsLeft ?? 0)
                                                 .arg(detail.event.waitlistedCount) : ""
                            variant: "muted"
                            font.pixelSize: Theme.fontXs
                        }
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        TfButton {
                            visible: detail.event && !detail.manage
                            text: detail.event && detail.event.full ? qsTr("Join the waitlist") : qsTr("Apply")
                            variant: "primary"
                            compact: true
                            enabled: detail.event && detail.event.canApply
                            ToolTip.visible: hovered && !enabled
                            ToolTip.text: detail.event ? (detail.event.myApplication && ["pending", "approved", "waitlisted"].indexOf(detail.event.myApplication.status) >= 0
                                                          ? qsTr("You have already applied.") : detail.event.closedReason) : ""
                            onClicked: applyDialog.openFor("event", detail.event.id, detail.event.name, detail.event.positions)
                        }
                        TfButton {
                            visible: detail.event && detail.event.myApplication
                                     && Hub.canWithdraw("event", detail.event.myApplication.status, detail.deadlinePassed)
                            text: qsTr("Withdraw")
                            variant: "danger"
                            compact: true
                            onClicked: {
                                confirm.pendingAction = () => Hub.post("/event-applications/" + detail.event.myApplication.id + "/withdraw", {}, (status, data) => {
                                    if (status === 200) { detail.notify(qsTr("Application withdrawn")); detail.load() }
                                    else detail.notify(data.error)
                                })
                                confirm.ask(qsTr("Withdraw your application?"),
                                            detail.event.myApplication.status === "approved" ? qsTr("Your place goes to someone else.") : qsTr("You can apply again while applications are open."),
                                            qsTr("Withdraw"))
                            }
                        }
                        TfButton {
                            visible: detail.event && detail.event.configured && !detail.manage
                            text: qsTr("Message organiser anonymously")
                            compact: true
                            onClicked: anonDialog.openFor("event", detail.event.id, detail.event.name)
                        }
                        TfButton {
                            visible: detail.event && detail.event.canConfigure
                            text: detail.event && detail.event.configured ? qsTr("Application settings") : qsTr("Open for applications")
                            variant: detail.event && detail.event.configured ? "secondary" : "primary"
                            compact: true
                            onClicked: settingsDialog.openFor(detail.event)
                        }
                    }
                    TfText {
                        visible: detail.event && detail.event.myApplication && (detail.event.myApplication.feedback ?? "") !== ""
                        text: detail.event && detail.event.myApplication ? qsTr("Organiser's message: “%1”").arg(detail.event.myApplication.feedback) : ""
                        variant: "secondary"
                        wrapMode: Text.WordWrap
                        elide: Text.ElideNone
                        Layout.fillWidth: true
                    }
                }
            }

            Segmented {
                visible: detail.manage
                Layout.leftMargin: 24
                options: [{ text: qsTr("Overview"), value: "overview" },
                          { text: qsTr("Applications  %1").arg(detail.event ? detail.event.pendingCount ?? 0 : 0), value: "applications" }]
                currentValue: detail.currentTab
                onActivated: value => detail.navigate("events/" + encodeURIComponent(detail.eventId) + (value === "overview" ? "" : "/" + value))
            }

            // ------------------------------------------------------------ overview
            GridLayout {
                visible: detail.currentTab === "overview"
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.bottomMargin: 24
                columns: detail.width > 1000 ? 2 : 1
                columnSpacing: Theme.gap
                rowSpacing: Theme.gap
                Panel {
                    title: qsTr("About the event")
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    ColumnLayout {
                        width: parent.width
                        spacing: 10
                        Repeater {
                            model: detail.event ? [
                                { label: qsTr("Description"), text: detail.event.description },
                                { label: qsTr("Requirements"), text: detail.event.requirements },
                                { label: qsTr("Eligibility"), text: detail.event.eligibility }
                            ].filter(f => f.text !== "") : []
                            delegate: ColumnLayout {
                                id: info
                                required property var modelData
                                spacing: 2
                                Layout.fillWidth: true
                                TfText { text: info.modelData.label; variant: "label" }
                                TfText { text: info.modelData.text; wrapMode: Text.WordWrap; elide: Text.ElideNone; Layout.fillWidth: true }
                            }
                        }
                        TfText {
                            visible: detail.event && !detail.event.description && !detail.event.requirements && !detail.event.eligibility
                            text: detail.event && detail.event.configured ? qsTr("The organiser has not added details yet.")
                                                                          : qsTr("This project is not open for applications yet.")
                            variant: "muted"
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
                Panel {
                    title: qsTr("Skills and positions")
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    ColumnLayout {
                        width: parent.width
                        spacing: 10
                        TfText { text: qsTr("Required skills (minimum level)"); variant: "label" }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 6
                            Repeater {
                                model: detail.event ? Object.keys(detail.event.requiredSkills) : []
                                delegate: Chip {
                                    required property string modelData
                                    text: Theme.skill(modelData) + " · " + detail.event.requiredSkills[modelData]
                                }
                            }
                        }
                        TfText { visible: detail.event && detail.event.positions.length > 0; text: qsTr("Available positions"); variant: "label" }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 6
                            Repeater {
                                model: detail.event ? detail.event.positions : []
                                delegate: Chip { required property string modelData; text: modelData; tone: "info"; large: true }
                            }
                        }
                    }
                }
            }

            // ------------------------------------------------------------ applications (organiser)
            ColumnLayout {
                visible: detail.currentTab === "applications"
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.bottomMargin: 24
                spacing: 10
                Segmented {
                    options: [
                        { text: qsTr("Pending  %1").arg(detail.apps.counts.pending ?? 0), value: "pending" },
                        { text: qsTr("Waitlisted  %1").arg(detail.apps.counts.waitlisted ?? 0), value: "waitlisted" },
                        { text: qsTr("Approved  %1").arg(detail.apps.counts.approved ?? 0), value: "approved" },
                        { text: qsTr("Rejected  %1").arg(detail.apps.counts.rejected ?? 0), value: "rejected" },
                        { text: qsTr("Withdrawn  %1").arg(detail.apps.counts.withdrawn ?? 0), value: "withdrawn" }
                    ]
                    currentValue: detail.appStatus
                    onActivated: value => { detail.appStatus = value; detail.loadApps(1) }
                }
                HubSkeleton { visible: detail.appsLoading && detail.apps.items.length === 0; rows: 2; Layout.fillWidth: true }
                EmptyState {
                    visible: !detail.appsLoading && detail.apps.items.length === 0
                    title: qsTr("No applications here")
                    message: qsTr("New applications appear here and in your notifications.")
                    Layout.fillWidth: true
                    Layout.topMargin: 20
                }
                Repeater {
                    model: detail.apps.items
                    delegate: ApplicationCard {
                        required property var modelData
                        application: modelData
                        reviewer: true
                        Layout.fillWidth: true
                        onDecide: (app, choice) => {
                            if (choice === "approve" && detail.event.full)
                                detail.notify(qsTr("The event is full. Raise the number of places or waitlist this applicant."))
                            else
                                decisionDialog.openFor(app, choice)
                        }
                        onOpenProfile: id => personDialog.show(id)
                    }
                }
                HubPager {
                    Layout.fillWidth: true
                    page: detail.apps.page; pages: detail.apps.pages; total: detail.apps.total
                    onPageRequested: p => detail.loadApps(p)
                }
            }
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Events: the projects hosts set up in TeamForge (hackathons, course projects ...). Those an
// organiser has opened take applications, with a deadline and a number of places.
Item {
    id: events

    signal navigate(string route)
    signal notify(string message)

    property var result: ({ items: [], page: 1, pages: 1, total: 0 })
    property bool loading: true
    property string error: ""
    property string query: ""
    property bool openOnly: false
    property bool mineOnly: false
    readonly property bool organiser: Hub.summary.canOrganize ?? false
    readonly property int columns: width > 1250 ? 3 : width > 720 ? 2 : 1
    readonly property bool narrow: width < 720

    function load(page) {
        loading = true
        Hub.get("/events?page=" + (page ?? 1) + "&q=" + encodeURIComponent(query) + (openOnly ? "&open=1" : "") + (mineOnly ? "&mine=1" : ""),
                (status, data) => {
            loading = false
            if (status === 200) { result = data; error = "" }
            else error = data.error ?? qsTr("Events could not be loaded.")
        })
    }
    Component.onCompleted: load(1)
    Timer { id: searchDelay; interval: 300; onTriggered: events.load(1) }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: events.narrow ? 14 : 24
        anchors.topMargin: events.narrow ? 14 : 18
        spacing: Theme.gap

        GridLayout {
            Layout.fillWidth: true
            columns: events.narrow ? 2 : 5
            columnSpacing: 10
            rowSpacing: 10
            TfTextField {
                Layout.columnSpan: events.narrow ? 2 : 1
                Layout.fillWidth: events.narrow
                placeholderText: qsTr("Search events or skills")
                Layout.preferredWidth: 320
                onTextChanged: { events.query = text.trim(); searchDelay.restart() }
            }
            Chip {
                large: true
                text: qsTr("Open for applications")
                tone: events.openOnly ? "success" : "muted"
                Accessible.role: Accessible.CheckBox
                Accessible.checked: events.openOnly
                Accessible.name: text
                Accessible.onPressAction: { events.openOnly = !events.openOnly; events.load(1) }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: { events.openOnly = !events.openOnly; events.load(1) } }
            }
            Chip {
                visible: events.organiser
                large: true
                text: qsTr("I organise")
                tone: events.mineOnly ? "info" : "muted"
                Accessible.role: Accessible.CheckBox
                Accessible.checked: events.mineOnly
                Accessible.name: text
                Accessible.onPressAction: { events.mineOnly = !events.mineOnly; events.load(1) }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: { events.mineOnly = !events.mineOnly; events.load(1) } }
            }
            Item { Layout.fillWidth: true; visible: !events.narrow }
        }

        HubSkeleton { visible: events.loading && events.result.items.length === 0; rows: 3; rowHeight: 150; Layout.fillWidth: true }
        ErrorBanner { message: events.error; Layout.fillWidth: true; onDismissed: events.error = "" }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            visible: events.result.items.length > 0
            ColumnLayout {
                width: parent.width
                spacing: Theme.gap
                GridLayout {
                    Layout.fillWidth: true
                    columns: events.columns
                    columnSpacing: Theme.gap
                    rowSpacing: Theme.gap
                    Repeater {
                        model: events.result.items
                        delegate: Surface {
                            id: card
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            Layout.alignment: Qt.AlignTop
                            implicitHeight: cardColumn.implicitHeight + 36
                            radius: Theme.radius
                            lifted: cardHover.hovered
                            border.color: cardHover.hovered ? Theme.borderStrong : Theme.border
                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Event %1").arg(modelData.name)
                            Accessible.onPressAction: events.navigate("events/" + encodeURIComponent(card.modelData.id))
                            Rectangle {
                                anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                                anchors.margins: 14
                                width: 3; radius: 1.5
                                color: Theme.typeColor(card.modelData.type)
                            }
                            ColumnLayout {
                                id: cardColumn
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 18
                                anchors.leftMargin: 28
                                spacing: 6
                                RowLayout {
                                    Layout.fillWidth: true
                                    Chip { text: card.modelData.type; tone: "tint"; tint: Theme.typeColor(card.modelData.type); visible: card.modelData.type !== "" }
                                    Item { Layout.fillWidth: true }
                                    HubStatus { visible: !!card.modelData.myStatus; status: card.modelData.myStatus ?? "" }
                                }
                                TfText { text: card.modelData.name; variant: "heading"; Layout.fillWidth: true }
                                TfText {
                                    text: card.modelData.summary
                                    variant: "secondary"
                                    wrapMode: Text.WordWrap
                                    elide: Text.ElideRight
                                    maximumLineCount: 2
                                    Layout.fillWidth: true
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Chip {
                                        text: card.modelData.open ? (card.modelData.full ? qsTr("Full · waitlist") : qsTr("Open"))
                                                                  : card.modelData.configured ? qsTr("Closed") : qsTr("Not open yet")
                                        tone: card.modelData.open ? (card.modelData.full ? "warning" : "success") : "muted"
                                    }
                                    TfText {
                                        text: [card.modelData.capacity !== null && card.modelData.capacity !== undefined
                                                   ? qsTr("%1 of %2 places taken").arg(card.modelData.approvedCount).arg(card.modelData.capacity) : "",
                                               card.modelData.open && card.modelData.deadline ? Hub.deadlineText(card.modelData.deadline) : ""]
                                              .filter(t => t !== "").join(" · ")
                                        variant: "muted"
                                        font.pixelSize: Theme.fontXs
                                        Layout.fillWidth: true
                                    }
                                    HubBadge { count: card.modelData.pendingCount ?? 0 }
                                }
                            }
                            HoverHandler { id: cardHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: events.navigate("events/" + encodeURIComponent(card.modelData.id)) }
                        }
                    }
                }
                HubPager {
                    Layout.fillWidth: true
                    page: events.result.page; pages: events.result.pages; total: events.result.total
                    onPageRequested: p => events.load(p)
                }
            }
        }
        EmptyState {
            visible: !events.loading && events.result.items.length === 0 && events.error === ""
            title: qsTr("No events match")
            message: events.organiser ? qsTr("Create projects in the Host workspace; then open them for applications here.")
                                      : qsTr("Try another search or filter.")
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}

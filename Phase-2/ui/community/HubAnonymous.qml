pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Anonymous connection requests. Inbox: requests to the teams the person leads, the events they
// organise, or to them; the sender is always "Anonymous". Sent: the person's own requests and
// their status. A conversation can be answered from either side; recipients can mark it reviewed,
// interested or declined, invite the sender to apply, report it or block the sender.
Item {
    id: anon

    property string threadId: ""
    signal navigate(string route)
    signal notify(string message)

    property string box: "inbox"
    property string status: ""
    property var list: ({ items: [], page: 1, pages: 1, total: 0 })
    property bool loadingList: true
    property var thread: null
    property bool loadingThread: false

    function loadList(page) {
        loadingList = true
        Hub.get("/anonymous/" + box + "?page=" + (page ?? 1) + (box === "inbox" && status !== "" ? "&status=" + status : ""), (code, data) => {
            loadingList = false
            if (code === 200) list = data
        })
    }
    function loadThread() {
        if (threadId === "") { thread = null; return }
        loadingThread = true
        Hub.get("/anonymous/" + threadId, (code, data) => {
            loadingThread = false
            if (code === 200) {
                thread = data.thread
                const wanted = thread.viewer === "sender" ? "sent" : "inbox"
                if (box !== wanted) { box = wanted; loadList(1) }
            } else {
                thread = null
                anon.notify(data.error ?? qsTr("This conversation could not be opened."))
            }
        })
    }
    function act(path, body, message) {
        Hub.post("/anonymous/" + threadId + path, body, (code, data) => {
            if (code === 200 || code === 201) {
                if (message) anon.notify(message)
                loadThread()
                loadList(list.page)
            } else {
                anon.notify(data.error ?? qsTr("That did not work."))
            }
        })
    }

    onThreadIdChanged: loadThread()
    Component.onCompleted: { loadList(1); loadThread() }

    HubDialog {
        id: reportDialog
        title: qsTr("Report this conversation")
        subtitle: qsTr("An admin reviews reported conversations. For that review only, they can see which account sent it.")
        primaryText: qsTr("Report")
        primaryVariant: "danger"
        primaryEnabled: reason.text.trim() !== ""
        onAccepted: {
            busy = true
            Hub.post("/anonymous/" + anon.threadId + "/report", { reason: reason.text }, (code, data) => {
                busy = false
                if (code === 201) { close(); anon.notify(qsTr("Reported. Thank you.")) }
                else error = data.error ?? qsTr("The report could not be sent.")
            })
        }
        TfTextArea { id: reason; maxLength: 500; placeholderText: qsTr("What is wrong? e.g. spam, harassment"); Layout.fillWidth: true }
    }
    ConfirmDialog {
        id: confirm
        property var pendingAction: null
        onConfirmed: if (pendingAction) pendingAction()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: Theme.gap

        // ---------------------------------------------------------------- list
        Surface {
            Layout.preferredWidth: 360
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                Segmented {
                    options: [{ text: qsTr("Inbox  %1").arg(Hub.anonymousInbox), value: "inbox" },
                              { text: qsTr("Sent  %1").arg(Hub.summary.anonymousSentUnread ?? 0), value: "sent" }]
                    currentValue: anon.box
                    onActivated: value => { anon.box = value; anon.loadList(1) }
                }
                Segmented {
                    visible: anon.box === "inbox"
                    options: [{ text: qsTr("All"), value: "" }, { text: qsTr("New"), value: "new" }, { text: qsTr("Interested"), value: "interested" },
                              { text: qsTr("Declined"), value: "declined" }]
                    currentValue: anon.status
                    onActivated: value => { anon.status = value; anon.loadList(1) }
                }
                HubSkeleton { visible: anon.loadingList && anon.list.items.length === 0; rows: 3; rowHeight: 70; Layout.fillWidth: true }
                EmptyState {
                    visible: !anon.loadingList && anon.list.items.length === 0
                    title: anon.box === "inbox" ? qsTr("No anonymous requests") : qsTr("Nothing sent yet")
                    message: anon.box === "inbox" ? qsTr("People can reach your teams and events without revealing who they are. Their requests arrive here.")
                                                  : qsTr("Use “Message anonymously” on a team, an event or a person to ask before you apply.")
                    actionText: anon.box === "sent" ? qsTr("People to contact") : ""
                    onActionTriggered: anon.navigate("people")
                    Layout.fillWidth: true
                    Layout.topMargin: 20
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    model: anon.list.items
                    delegate: Rectangle {
                        id: row
                        required property var modelData
                        readonly property bool selected: modelData.id === anon.threadId
                        width: ListView.view.width
                        height: rowColumn.implicitHeight + 20
                        radius: Theme.radiusSmall
                        color: selected ? Theme.surfaceRaised : rowHover.hovered ? Theme.surfaceHover : "transparent"
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.target.name + ": " + modelData.preview
                        Accessible.onPressAction: anon.navigate("anonymous/" + row.modelData.id)
                        ColumnLayout {
                            id: rowColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 10
                            spacing: 2
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Rectangle { visible: row.modelData.unread; width: 8; height: 8; radius: 4; color: Theme.violet }
                                TfText {
                                    text: anon.box === "inbox" ? qsTr("Anonymous → %1").arg(row.modelData.target.name)
                                                               : qsTr("To %1").arg(row.modelData.target.name)
                                    font.weight: Font.Bold
                                    Layout.fillWidth: true
                                }
                            }
                            TfText { text: row.modelData.preview.replace(/\n/g, " "); variant: "muted"; Layout.fillWidth: true }
                            RowLayout {
                                Layout.fillWidth: true
                                TfText {
                                    text: Hub.relativeTime(row.modelData.lastMessageAt) + " · " + Theme.count(row.modelData.messageCount, qsTr("message"), qsTr("messages"))
                                          + (row.modelData.invited ? " · " + qsTr("invited to apply") : "")
                                    variant: "muted"
                                    font.pixelSize: 12
                                    Layout.fillWidth: true
                                }
                                HubStatus { status: row.modelData.status }
                            }
                        }
                        HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: anon.navigate("anonymous/" + row.modelData.id) }
                    }
                }
                HubPager {
                    Layout.fillWidth: true
                    page: anon.list.page; pages: anon.list.pages; total: anon.list.total
                    onPageRequested: p => anon.loadList(p)
                }
            }
        }

        // ---------------------------------------------------------------- conversation
        Surface {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(460, parent.width - 48)
                visible: !anon.thread && !anon.loadingThread
                spacing: 14
                EmptyState {
                    title: qsTr("Anonymous conversations")
                    message: qsTr("Choose a conversation on the left.")
                    Layout.fillWidth: true
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: notice.implicitHeight + 24
                    radius: Theme.radiusSmall
                    color: Qt.rgba(Theme.violet.r, Theme.violet.g, Theme.violet.b, 0.12)
                    border.color: Qt.rgba(Theme.violet.r, Theme.violet.g, Theme.violet.b, 0.4)
                    TfText {
                        id: notice
                        anchors.fill: parent
                        anchors.margins: 12
                        text: qsTr("What anonymity means") + "\n" + Hub.anonymityNotice
                        color: "#DCD3FF"
                        font.pixelSize: Theme.fontXs
                        wrapMode: Text.WordWrap
                        elide: Text.ElideNone
                    }
                }
            }
            HubSkeleton { anchors.fill: parent; anchors.margins: 16; visible: anon.loadingThread && !anon.thread; rows: 3 }

            ColumnLayout {
                anchors.fill: parent
                visible: !!anon.thread
                spacing: 0

                // header
                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 14
                    spacing: 10
                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        TfText {
                            text: !anon.thread ? "" : anon.thread.viewer === "recipient" ? qsTr("Anonymous → %1").arg(anon.thread.target.name)
                                                                                         : qsTr("You (anonymous) → %1").arg(anon.thread.target.name)
                            variant: "heading"
                            Layout.fillWidth: true
                        }
                        TfText {
                            text: !anon.thread ? "" : (anon.thread.target.kind === "team" ? qsTr("Team") : anon.thread.target.kind === "event" ? qsTr("Event") : qsTr("Person"))
                                                     + " · " + qsTr("started %1").arg(Hub.relativeTime(anon.thread.createdAt))
                                                     + (anon.thread.invited ? " · " + qsTr("invited to apply") : "")
                            variant: "muted"
                            font.pixelSize: Theme.fontXs
                        }
                    }
                    HubStatus { status: anon.thread ? anon.thread.status : "" }
                }
                // recipient actions
                Flow {
                    visible: !!anon.thread && anon.thread.viewer === "recipient"
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.bottomMargin: 10
                    spacing: 6
                    TfButton { text: qsTr("Mark reviewed"); compact: true; variant: "ghost"; enabled: !!anon.thread && anon.thread.status === "new"; onClicked: anon.act("/status", { status: "reviewed" }, qsTr("Marked as reviewed")) }
                    TfButton { text: qsTr("Interested"); compact: true; enabled: !!anon.thread && anon.thread.status !== "interested"; onClicked: anon.act("/status", { status: "interested" }, qsTr("The sender is told you're interested")) }
                    TfButton {
                        visible: !!anon.thread && anon.thread.canInvite
                        text: anon.thread && anon.thread.invited ? qsTr("Invite again") : qsTr("Invite to apply")
                        compact: true
                        variant: "primary"
                        onClicked: anon.act("/invite", {}, qsTr("Invitation sent. If they apply, you'll see their profile."))
                    }
                    TfButton { text: qsTr("Decline"); compact: true; variant: "ghost"; enabled: !!anon.thread && anon.thread.status !== "declined"; onClicked: anon.act("/status", { status: "declined" }, qsTr("Declined")) }
                    TfButton {
                        text: qsTr("Block sender")
                        compact: true
                        variant: "danger"
                        onClicked: {
                            confirm.pendingAction = () => anon.act("/block", {}, qsTr("Blocked. They can no longer message this recipient."))
                            confirm.ask(qsTr("Block this sender?"), qsTr("They will not be able to send more anonymous messages here. You still won't see who they are."), qsTr("Block"))
                        }
                    }
                    TfButton { text: qsTr("Report"); compact: true; variant: "danger"; onClicked: { reason.clear(); reportDialog.open() } }
                }
                TfButton {
                    visible: !!anon.thread && anon.thread.viewer === "sender"
                    text: qsTr("Report")
                    compact: true
                    variant: "ghost"
                    Layout.leftMargin: 14
                    Layout.bottomMargin: 10
                    onClicked: { reason.clear(); reportDialog.open() }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                // messages
                ListView {
                    id: threadView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 14
                    clip: true
                    spacing: 10
                    model: anon.thread ? anon.thread.messages : []
                    onCountChanged: Qt.callLater(positionViewAtEnd)
                    delegate: ColumnLayout {
                        id: msg
                        required property var modelData
                        width: ListView.view.width
                        spacing: 3
                        RowLayout {
                            Layout.alignment: msg.modelData.mine ? Qt.AlignRight : Qt.AlignLeft
                            spacing: 6
                            Rectangle {
                                visible: msg.modelData.author.anonymous && !msg.modelData.mine
                                width: 22; height: 22; radius: 11
                                color: Qt.rgba(Theme.violet.r, Theme.violet.g, Theme.violet.b, 0.3)
                                TfText { anchors.centerIn: parent; text: "?"; color: Theme.violet; font.weight: Font.ExtraBold; font.pixelSize: 12 }
                            }
                            HubAvatar {
                                visible: !msg.modelData.author.anonymous && !msg.modelData.mine
                                person: msg.modelData.author
                                size: 22
                            }
                            TfText { text: msg.modelData.author.name; font.weight: Font.Bold; font.pixelSize: Theme.fontXs; color: Theme.textSecondary }
                            TfText { text: Hub.relativeTime(msg.modelData.createdAt); variant: "muted"; font.pixelSize: 12 }
                        }
                        Rectangle {
                            Layout.alignment: msg.modelData.mine ? Qt.AlignRight : Qt.AlignLeft
                            implicitWidth: Math.min(msgText.implicitWidth + 24, threadView.width * 0.75)
                            implicitHeight: msgText.implicitHeight + 16
                            radius: 14
                            color: msg.modelData.mine ? Theme.accent : msg.modelData.fromSender ? Qt.rgba(Theme.violet.r, Theme.violet.g, Theme.violet.b, 0.16) : Theme.surfaceRaised
                            TextEdit {
                                id: msgText
                                anchors.fill: parent
                                anchors.margins: 8
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                text: msg.modelData.body
                                textFormat: TextEdit.PlainText
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.Wrap
                                color: msg.modelData.mine ? "#FFFFFF" : Theme.text
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontBase
                            }
                        }
                    }
                }

                // reply
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
                TfText {
                    visible: !!anon.thread && !anon.thread.canReply
                    text: qsTr("This conversation is closed.")
                    variant: "muted"
                    Layout.margins: 14
                }
                RowLayout {
                    visible: !!anon.thread && anon.thread.canReply
                    Layout.fillWidth: true
                    Layout.margins: 12
                    spacing: 10
                    TfTextArea {
                        id: reply
                        maxLength: 1000
                        minimumHeight: 48
                        placeholderText: anon.thread && anon.thread.viewer === "sender" ? qsTr("Reply (you stay anonymous)") : qsTr("Reply to the sender")
                        Layout.fillWidth: true
                    }
                    TfButton {
                        text: qsTr("Send")
                        variant: "primary"
                        enabled: reply.text.trim() !== "" && !reply.tooLong
                        Layout.alignment: Qt.AlignTop
                        onClicked: {
                            const problem = Hub.checkText(reply.text, 1000, qsTr("Message"), true)
                            if (problem !== "") { anon.notify(problem); return }
                            const text = reply.text
                            reply.clear()
                            Hub.post("/anonymous/" + anon.threadId + "/messages", { body: text }, (code, data) => {
                                if (code === 201) anon.loadThread()
                                else { reply.text = text; anon.notify(data.error ?? qsTr("The reply could not be sent.")) }
                            })
                        }
                    }
                }
            }
        }
    }
}

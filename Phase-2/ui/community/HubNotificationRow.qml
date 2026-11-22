import QtQuick
import QtQuick.Layouts
import TeamForge

// One notification: unread dot, title, short text, time. Clicking opens what it is about.
Rectangle {
    id: row

    property var notification: ({})
    signal opened()

    readonly property bool unread: !(notification.read ?? true)
    readonly property color kindColor: {
        const kind = notification.kind ?? ""
        if (kind.indexOf("approved") >= 0 || kind === "invitation") return Theme.success
        if (kind.indexOf("rejected") >= 0 || kind === "team_removed" || kind === "report") return Theme.accentHover
        if (kind.indexOf("anonymous") === 0) return Theme.violet
        if (kind === "team_message") return Theme.info
        return Theme.warning
    }

    implicitHeight: layout.implicitHeight + 20
    radius: Theme.radiusSmall
    color: hover.hovered ? Theme.surfaceRaised : unread ? Qt.rgba(1, 1, 1, 0.03) : "transparent"
    Accessible.role: Accessible.Button
    Accessible.name: (unread ? qsTr("Unread: ") : "") + (notification.title ?? "")
    Accessible.onPressAction: row.opened()

    RowLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 10
        Rectangle {
            width: 8; height: 8; radius: 4
            color: row.unread ? row.kindColor : "transparent"
            border.color: row.unread ? "transparent" : Theme.borderStrong
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 7
        }
        ColumnLayout {
            spacing: 1
            Layout.fillWidth: true
            TfText {
                text: row.notification.title ?? ""
                font.weight: row.unread ? Font.Bold : Font.Medium
                color: row.unread ? Theme.text : Theme.textSecondary
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
                maximumLineCount: 2
                Layout.fillWidth: true
            }
            TfText {
                visible: (row.notification.body ?? "") !== ""
                text: (row.notification.body ?? "").replace(/\n/g, " ")
                variant: "muted"
                font.pixelSize: Theme.fontXs
                Layout.fillWidth: true
            }
        }
        TfText {
            text: Hub.relativeTime(row.notification.createdAt ?? 0)
            variant: "muted"
            font.pixelSize: Theme.fontXs
            Layout.alignment: Qt.AlignTop
        }
    }
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: row.opened() }
}

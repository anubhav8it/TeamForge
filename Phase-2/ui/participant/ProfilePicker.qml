pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// "Find my profile": a participant already in the directory picks their own profile instead of
// creating a new one (Backend.claimProfile). Search runs in C++.
Popup {
    id: picker

    signal picked()
    property string query: ""
    readonly property var results: opened ? Backend.searchStudents(query, "", "name").slice(0, 60) : []

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(560, parent.width - 48)
    height: Math.min(620, parent.height - 80)
    modal: true
    dim: true
    padding: 20
    onOpened: { search.clear(); search.forceActiveFocus() }

    Overlay.modal: Rectangle { color: "#990B0D11" }
    background: Rectangle {
        radius: Theme.radius
        color: Theme.surfaceRaised
        border.color: Theme.borderStrong
    }

    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            TfText {
                text: qsTr("Find my profile")
                variant: "section"
                font.pixelSize: 20
                Layout.fillWidth: true
            }
            TfButton {
                text: "✕"
                variant: "ghost"
                compact: true
                Accessible.name: qsTr("Close")
                onClicked: picker.close()
            }
        }
        TfTextField {
            id: search
            placeholderText: qsTr("Search by your name")
            Layout.fillWidth: true
            onTextChanged: delay.restart()
            Timer {
                id: delay
                interval: 150
                onTriggered: picker.query = search.text
            }
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: picker.results
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: Rectangle {
                id: row
                required property var modelData
                width: ListView.view.width
                implicitHeight: 60
                radius: Theme.radiusSmall
                color: rowHover.hovered ? Theme.surfaceHover : "transparent"
                Accessible.role: Accessible.Button
                Accessible.name: modelData.name
                Accessible.onPressAction: row.choose()
                function choose() {
                    if (Backend.claimProfile(row.modelData.id)) {
                        picker.close()
                        picker.picked()
                    }
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 12
                    Avatar {
                        name: row.modelData.name
                        seed: row.modelData.id
                        size: 40
                    }
                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        TfText {
                            text: row.modelData.name
                            font.weight: Font.ExtraBold
                            Layout.fillWidth: true
                        }
                        TfText {
                            text: row.modelData.program || row.modelData.summary
                            variant: "muted"
                            font.pixelSize: Theme.fontXs
                            Layout.fillWidth: true
                        }
                    }
                    TfText {
                        text: qsTr("This is me")
                        font.weight: Font.ExtraBold
                        color: rowHover.hovered ? Theme.info : Theme.textMuted
                    }
                }
                HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: row.choose() }
            }
        }
    }
}

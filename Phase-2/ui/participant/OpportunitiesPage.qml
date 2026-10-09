pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Every project as an opportunity, best fit first (Backend.opportunities: the same engine and
// weights hosts use). Filtering by type or text only narrows what is displayed.
TfPage {
    id: page

    signal viewOpportunity(string requirementId)
    signal whyThisMatch(string requirementId)
    signal expressInterest(string requirementId)

    property string typeFilter: ""
    property string query: ""

    readonly property var all: Backend.opportunities
    readonly property var types: [...new Set(all.map(o => o.type).filter(t => t))]
    readonly property var shown: all.filter(o => (typeFilter === "" || o.type === typeFilter)
        && (query === "" || o.name.toLowerCase().includes(query) || (o.summary ?? "").toLowerCase().includes(query)))
    readonly property int columns: width >= 1400 ? 3 : 2

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        anchors.topMargin: 20
        spacing: Theme.gap

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            TfTextField {
                id: search
                placeholderText: qsTr("Search projects")
                Layout.preferredWidth: 320
                onTextChanged: page.query = text.trim().toLowerCase()
            }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: [""].concat(page.types)
                    delegate: Chip {
                        id: typeChip
                        required property string modelData
                        readonly property bool chosen: page.typeFilter === modelData
                        large: true
                        text: modelData === "" ? qsTr("All types") : modelData
                        tone: chosen ? "tint" : "muted"
                        tint: modelData === "" ? Theme.text : Theme.typeColor(modelData)
                        Accessible.role: Accessible.RadioButton
                        Accessible.name: text
                        Accessible.checked: chosen
                        Accessible.onPressAction: page.typeFilter = typeChip.modelData
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: page.typeFilter = typeChip.modelData }
                    }
                }
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            GridLayout {
                width: parent.width
                columns: page.columns
                columnSpacing: Theme.gap
                rowSpacing: Theme.gap

                Repeater {
                    model: page.shown
                    delegate: OpportunityCard {
                        required property var modelData
                        opportunity: modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.alignment: Qt.AlignTop
                        onViewOpportunity: id => page.viewOpportunity(id)
                        onWhyThisMatch: id => page.whyThisMatch(id)
                        onExpressInterest: id => page.expressInterest(id)
                    }
                }
            }
        }

        EmptyState {
            visible: page.shown.length === 0
            title: qsTr("No opportunities match")
            message: qsTr("Try another type or search.")
            Layout.fillWidth: true
            Layout.bottomMargin: 80
        }
    }
}

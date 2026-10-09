pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// The whole skill universe (Backend.querySkills): every skill offered, required or catalogued,
// searchable by name and category and sortable by usage. Selecting one shows its counts,
// related skills (BFS over SkillGraph), its state in SkillIndex / SkillBST / SkillGraph and the
// safe edits: category, rename or merge, delete.
TfPage {
    id: page

    signal inspect(string kind, string id)
    signal notify(string message)

    property string query: ""
    property string category: ""
    property string sort: "participants"
    property var result: ({})
    property string selected: ""
    property bool showStructures: false
    readonly property var detail: selected !== "" && Backend.skillCount >= 0 ? Backend.skillDetail(selected) : ({})
    readonly property var categories: Backend.skillCategories

    function requery() {
        result = Backend.querySkills({ query: query, category: category, sort: sort })
        if (selected === "" && (result.rows ?? []).length > 0)
            selected = result.rows[0].name
    }
    onActiveChanged: if (active) requery()
    onCategoryChanged: requery()
    onSortChanged: requery()
    Timer {
        id: searchDelay
        interval: 150
        onTriggered: page.requery()
    }
    Connections {
        target: Backend
        function onDataChanged() {
            if (!page.active)
                return
            page.requery()
            if (page.selected !== "" && Backend.skillDetail(page.selected).name === undefined)
                page.selected = ""
        }
    }

    ConfirmDialog {
        id: confirmDelete
        onConfirmed: {
            const name = page.selected
            if (Backend.deleteSkill(name)) {
                page.selected = ""
                page.requery()
                page.notify(qsTr("%1 deleted everywhere (unsaved)").arg(Theme.skill(name)))
            }
        }
    }
    ConfirmDialog {
        id: confirmRename
        property string target
        onConfirmed: {
            const next = target.trim().toLowerCase()
            if (Backend.renameSkill(page.selected, target)) {
                page.notify(qsTr("Renamed to %1 (unsaved)").arg(Theme.skill(next)))
                page.requery()
                page.selected = Backend.skillDetail(target).name ?? ""
            }
        }
    }

    // New skill
    Popup {
        id: createPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 440
        modal: true
        dim: true
        padding: 24
        Overlay.modal: Rectangle { color: "#990B0D11" }
        background: Rectangle { radius: Theme.radius; color: Theme.surfaceRaised; border.color: Theme.borderStrong }
        onOpened: { newName.text = ""; newCategory.editText = ""; newName.forceActiveFocus() }
        contentItem: ColumnLayout {
            spacing: 12
            TfText { text: qsTr("New skill"); variant: "section" }
            TfText {
                text: qsTr("Skills are open-ended: any name works. Catalogued skills appear in autocomplete straight away.")
                variant: "secondary"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            SkillField { id: newName; placeholderText: qsTr("Skill name, e.g. ROS2"); Layout.fillWidth: true }
            TfComboBox {
                id: newCategory
                editable: true
                model: page.categories
                Layout.fillWidth: true
                Accessible.name: qsTr("Category")
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                TfButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: createPopup.close() }
                TfButton {
                    text: qsTr("Create skill")
                    variant: "primary"
                    enabled: newName.text.trim() !== ""
                    onClicked: {
                        if (Backend.saveSkill(newName.text, newCategory.editText)) {
                            createPopup.close()
                            page.requery()
                            page.selected = Backend.skillDetail(newName.text).name ?? ""
                            page.notify(qsTr("Skill created (unsaved)"))
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        anchors.topMargin: 20
        spacing: 14

        // Headline: the real total.
        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            TfText {
                text: String(page.result.total ?? Backend.skillCount)
                font.pixelSize: 44
                font.weight: Font.ExtraBold
                font.letterSpacing: -1
            }
            ColumnLayout {
                spacing: 0
                TfText { text: qsTr("SKILLS"); font.pixelSize: Theme.fontLg; font.weight: Font.ExtraBold; font.letterSpacing: 2 }
                TfText {
                    text: qsTr("%1 offered by participants · %2 categories · open-ended")
                          .arg(Backend.systemStatus.offeredSkills ?? 0).arg(page.categories.length)
                    variant: "muted"
                    font.weight: Font.Bold
                }
            }
            Item { Layout.fillWidth: true }
            TfButton {
                text: qsTr("+ New skill")
                variant: "primary"
                onClicked: createPopup.open()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            TfTextField {
                placeholderText: qsTr("Search skills")
                Layout.preferredWidth: 260
                onTextChanged: { page.query = text; searchDelay.restart() }
            }
            TfComboBox {
                Layout.preferredWidth: 240
                model: [qsTr("All categories")].concat(page.categories).concat([qsTr("Uncategorised")])
                Accessible.name: qsTr("Category filter")
                onActivated: page.category = currentIndex === 0 ? "" : currentText
            }
            Segmented {
                options: [{ text: qsTr("Most participants"), value: "participants" }, { text: qsTr("Most projects"), value: "projects" },
                          { text: qsTr("A–Z"), value: "name" }]
                currentValue: page.sort
                onActivated: value => page.sort = value
            }
            Item { Layout.fillWidth: true }
            TfText {
                text: qsTr("%1 shown").arg(page.result.count ?? 0)
                variant: "muted"
                font.weight: Font.Bold
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.gap

            // Skill table
            Panel {
                padded: false
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.pad
                        Layout.rightMargin: Theme.pad + 10
                        Layout.topMargin: 14
                        Layout.bottomMargin: 10
                        spacing: 12
                        TfText { text: qsTr("NO."); variant: "label"; Layout.preferredWidth: 52 }
                        TfText { text: qsTr("SKILL"); variant: "label"; Layout.fillWidth: true }
                        TfText { text: qsTr("CATEGORY"); variant: "label"; Layout.preferredWidth: 170 }
                        TfText { text: qsTr("PEOPLE"); variant: "label"; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 70 }
                        TfText { text: qsTr("PROJECTS"); variant: "label"; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 80 }
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
                    ListView {
                        id: list
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: page.result.rows ?? []
                        reuseItems: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }
                        delegate: Rectangle {
                            id: row
                            required property var modelData
                            required property int index
                            readonly property bool chosen: page.selected === modelData.name
                            width: ListView.view.width
                            height: 46
                            color: chosen ? Theme.selection : rowHover.hovered ? Theme.surfaceRaised
                                 : index % 2 === 0 ? "transparent" : Qt.rgba(1, 1, 1, 0.018)
                            Accessible.role: Accessible.ListItem
                            Accessible.name: Theme.skill(modelData.name)
                            Accessible.onPressAction: page.selected = row.modelData.name
                            Rectangle { visible: row.chosen; width: 3; height: parent.height; color: Theme.violet }
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.pad
                                anchors.rightMargin: Theme.pad + 10
                                spacing: 12
                                TfText {
                                    text: "#" + String(row.modelData.number).padStart(3, "0")
                                    font.weight: Font.Bold
                                    color: Theme.textMuted
                                    Layout.preferredWidth: 52
                                }
                                TfText {
                                    text: Theme.skill(row.modelData.name)
                                    font.weight: Font.ExtraBold
                                    font.letterSpacing: 0.4
                                    Layout.fillWidth: true
                                }
                                TfText {
                                    text: row.modelData.category
                                    variant: row.modelData.category === "Uncategorised" ? "muted" : "secondary"
                                    Layout.preferredWidth: 170
                                }
                                TfText {
                                    text: row.modelData.participants
                                    font.weight: Font.Bold
                                    color: row.modelData.participants > 0 ? Theme.text : Theme.textMuted
                                    horizontalAlignment: Text.AlignRight
                                    Layout.preferredWidth: 70
                                }
                                TfText {
                                    text: row.modelData.projects
                                    font.weight: Font.Bold
                                    color: row.modelData.projects > 0 ? Theme.accentHover : Theme.textMuted
                                    horizontalAlignment: Text.AlignRight
                                    Layout.preferredWidth: 80
                                }
                            }
                            HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: page.selected = row.modelData.name }
                        }
                        EmptyState {
                            anchors.centerIn: parent
                            width: 320
                            visible: list.count === 0
                            title: qsTr("No skills match")
                        }
                    }
                }
            }

            // Selected skill
            Panel {
                Layout.preferredWidth: Math.min(460, page.width * 0.34)
                Layout.fillHeight: true
                visible: page.detail.name !== undefined

                ScrollView {
                    anchors.fill: parent
                    contentWidth: availableWidth
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: 16

                        ColumnLayout {
                            spacing: 6
                            Layout.fillWidth: true
                            TfText {
                                text: Theme.skill(page.detail.name ?? "")
                                font.pixelSize: 26
                                font.weight: Font.ExtraBold
                                font.letterSpacing: 0.6
                                wrapMode: Text.WrapAnywhere
                                Layout.fillWidth: true
                            }
                            RowLayout {
                                spacing: 8
                                Chip { text: page.detail.category ?? ""; tone: "tint"; tint: Theme.violet }

                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Repeater {
                                model: [[qsTr("People"), page.detail.participants ?? 0, Theme.info],
                                        [qsTr("Projects"), page.detail.projectCount ?? 0, Theme.accent],
                                        [qsTr("Learning"), page.detail.learners ?? 0, Theme.teal]]
                                delegate: StatTile {
                                    required property var modelData
                                    label: modelData[0]
                                    value: String(modelData[1])
                                    accentColor: modelData[2]
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        // Related skills
                        ColumnLayout {
                            visible: (page.detail.related ?? []).length > 0
                            spacing: 8
                            Layout.fillWidth: true
                            TfText { text: qsTr("Related skills"); variant: "heading" }
                            TfText { text: qsTr("Offered together most often (SkillGraph neighbours)"); variant: "muted" }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 6
                                Repeater {
                                    model: page.detail.related ?? []
                                    delegate: Chip {
                                        id: relatedChip
                                        required property var modelData
                                        skill: true
                                        text: modelData.skill
                                        Accessible.role: Accessible.Button
                                        Accessible.name: Theme.skill(modelData.skill)
                                        Accessible.onPressAction: page.selected = relatedChip.modelData.skill
                                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                                        TapHandler { onTapped: page.selected = relatedChip.modelData.skill }
                                    }
                                }
                            }
                        }

                        // Projects and holders
                        ColumnLayout {
                            visible: (page.detail.projects ?? []).length > 0
                            spacing: 6
                            Layout.fillWidth: true
                            TfText { text: qsTr("Required by"); variant: "heading" }
                            Repeater {
                                model: page.detail.projects ?? []
                                delegate: TfText {
                                    required property var modelData
                                    text: qsTr("%1 · level %2+").arg(modelData.name).arg(modelData.minLevel)
                                    variant: "secondary"
                                    font.weight: Font.Bold
                                }
                            }
                        }
                        ColumnLayout {
                            visible: (page.detail.holders ?? []).length > 0
                            spacing: 4
                            Layout.fillWidth: true
                            TfText { text: qsTr("Strongest holders"); variant: "heading" }
                            Repeater {
                                model: page.detail.holders ?? []
                                delegate: Rectangle {
                                    id: holder
                                    required property var modelData
                                    Layout.fillWidth: true
                                    implicitHeight: 42
                                    radius: Theme.radiusSmall
                                    color: holderHover.hovered ? Theme.surfaceRaised : "transparent"
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        spacing: 10
                                        Avatar { name: holder.modelData.name; seed: holder.modelData.id; size: 28 }
                                        TfText { text: holder.modelData.name; font.weight: Font.Bold; Layout.fillWidth: true }
                                        LevelPips { level: holder.modelData.level }
                                    }
                                    HoverHandler { id: holderHover; cursorShape: Qt.PointingHandCursor }
                                    TapHandler { onTapped: page.inspect("participant", holder.modelData.id) }
                                }
                            }
                        }

                        // Edits
                        ColumnLayout {
                            spacing: 8
                            Layout.fillWidth: true
                            TfText { text: qsTr("Edit"); variant: "heading" }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                TfComboBox {
                                    id: categoryBox
                                    editable: true
                                    model: page.categories
                                    Layout.fillWidth: true
                                    Accessible.name: qsTr("Skill category")
                                    Connections {
                                        target: page
                                        function onDetailChanged() {
                                            const current = page.detail.category === "Uncategorised" ? "" : (page.detail.category ?? "")
                                            categoryBox.currentIndex = categoryBox.find(current)
                                            categoryBox.editText = current
                                        }
                                    }
                                }
                                TfButton {
                                    text: qsTr("Set category")
                                    compact: true
                                    onClicked: if (Backend.saveSkill(page.selected, categoryBox.editText)) page.notify(qsTr("Category updated (unsaved)"))
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                SkillField {
                                    id: renameField
                                    placeholderText: qsTr("Rename or merge into…")
                                    Layout.fillWidth: true
                                }
                                TfButton {
                                    text: qsTr("Rename")
                                    compact: true
                                    enabled: renameField.text.trim() !== ""
                                    onClicked: {
                                        confirmRename.target = renameField.text
                                        confirmRename.ask(qsTr("Rename %1?").arg(Theme.skill(page.selected)),
                                                          qsTr("Every profile, learning interest and project using it changes to %1. If that skill exists, the two are merged and the higher level is kept.")
                                                              .arg(Theme.skill(renameField.text.trim())),
                                                          qsTr("Rename skill"))
                                    }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                TfButton {
                                    text: qsTr("Raw record")
                                    compact: true
                                    onClicked: page.inspect("skill", page.selected)
                                }
                                Item { Layout.fillWidth: true }
                                TfButton {
                                    text: qsTr("Delete skill")
                                    variant: "danger"
                                    compact: true
                                    onClicked: confirmDelete.ask(qsTr("Delete %1?").arg(Theme.skill(page.selected)),
                                                                 qsTr("It is removed from %1 and %2. A profile or project that has no other skill blocks the delete.")
                                                                     .arg(Theme.count(page.detail.participants ?? 0, qsTr("profile"), qsTr("profiles")))
                                                                     .arg(Theme.count(page.detail.projectCount ?? 0, qsTr("project"), qsTr("projects"))),
                                                                 qsTr("Delete skill"))
                                }
                            }
                        }
                        // Advanced: where the skill sits in the data structures (collapsed by default).
                        ColumnLayout {
                            spacing: 6
                            Layout.fillWidth: true
                            RowLayout {
                                spacing: 8
                                Layout.fillWidth: true
                                TfText { text: qsTr("Advanced"); variant: "heading"; Layout.fillWidth: true }
                                Chip {
                                    visible: !page.detail.consistent
                                    text: qsTr("Needs attention")
                                    tone: "accent"
                                }
                                TfButton {
                                    text: page.showStructures ? qsTr("Hide") : qsTr("Data structures")
                                    variant: "ghost"
                                    compact: true
                                    onClicked: page.showStructures = !page.showStructures
                                }
                            }
                            Repeater {
                                model: [[qsTr("SkillIndex"), page.detail.inIndex, qsTr("%1 participant ids").arg(page.detail.indexEntries ?? 0)],
                                        [qsTr("SkillBST (AVL)"), page.detail.inTree, qsTr("count %1").arg(page.detail.treeCount ?? 0)],
                                        [qsTr("SkillGraph"), page.detail.inGraph, qsTr("%1 neighbours").arg(page.detail.degree ?? 0)]]
                                delegate: RowLayout {
                                    id: structure
                                    required property var modelData
                                    visible: page.showStructures
                                    spacing: 10
                                    Layout.fillWidth: true
                                    Rectangle {
                                        implicitWidth: 10
                                        implicitHeight: 10
                                        radius: 5
                                        color: structure.modelData[1] ? Theme.success : Theme.textMuted
                                    }
                                    TfText { text: structure.modelData[0]; font.weight: Font.Bold; Layout.preferredWidth: 130 }
                                    TfText {
                                        text: structure.modelData[1] ? structure.modelData[2] : qsTr("not present (nobody offers it)")
                                        variant: "secondary"
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 4 }
                    }
                }
            }
        }
    }
}

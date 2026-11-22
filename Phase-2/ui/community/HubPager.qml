import QtQuick
import QtQuick.Layouts
import TeamForge

// Previous / next for a paged list from the server ({page, pages, total}).
RowLayout {
    id: pager
    property int page: 1
    property int pages: 1
    property int total: 0
    signal pageRequested(int page)

    visible: pages > 1
    spacing: 10
    Item { Layout.fillWidth: true }
    TfButton {
        text: qsTr("‹ Previous")
        compact: true
        variant: "ghost"
        enabled: pager.page > 1
        onClicked: pager.pageRequested(pager.page - 1)
    }
    TfText {
        text: qsTr("Page %1 of %2 · %3 in all").arg(pager.page).arg(pager.pages).arg(pager.total)
        variant: "muted"
    }
    TfButton {
        text: qsTr("Next ›")
        compact: true
        variant: "ghost"
        enabled: pager.page < pager.pages
        onClicked: pager.pageRequested(pager.page + 1)
    }
    Item { Layout.fillWidth: true }
}

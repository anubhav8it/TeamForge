import QtQuick
import TeamForge

// Developer workspace: Overview, Participants, Projects, Skills and Workspace (previews, local
// data and system tools), plus the record inspector they share. Every action here is also
// checked by the backend, which rejects it in the participant and host workspaces.
Item {
    id: workspace

    property int currentPage: 0
    signal navigate(int index)
    signal notify(string message)
    signal explainFit(string studentId, string requirementId)

    function inspect(kind, id) {
        inspector.show(kind, id)
    }

    DevOverviewPage {
        anchors.fill: parent
        active: workspace.currentPage === 0
        onOpenPage: index => workspace.navigate(index)
        onNotify: message => workspace.notify(message)
    }
    DevParticipantsPage {
        anchors.fill: parent
        active: workspace.currentPage === 1
        onInspect: (kind, id) => workspace.inspect(kind, id)
        onNotify: message => workspace.notify(message)
    }
    ProjectsPage {
        anchors.fill: parent
        developerMode: true
        active: workspace.currentPage === 2
        onInspect: (kind, id) => workspace.inspect(kind, id)
        onExplainFit: (studentId, requirementId) => workspace.explainFit(studentId, requirementId)
        onNotify: message => workspace.notify(message)
    }
    DevSkillsPage {
        anchors.fill: parent
        active: workspace.currentPage === 3
        onInspect: (kind, id) => workspace.inspect(kind, id)
        onNotify: message => workspace.notify(message)
    }
    DevWorkspacePage {
        anchors.fill: parent
        active: workspace.currentPage === 4
        onNotify: message => workspace.notify(message)
    }

    RecordInspector {
        id: inspector
        onNotify: message => workspace.notify(message)
    }
}

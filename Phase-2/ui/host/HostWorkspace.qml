import QtQuick
import TeamForge

// Host workspace: Overview -> Projects -> Applicants -> Participants -> Matching (team building
// lives in Matching). Created only while the host workspace is active.
Item {
    id: workspace

    property int currentPage: 0
    property string explainedId: ""
    signal navigate(int index)
    signal explain(string studentId)
    signal explainFit(string studentId, string requirementId)
    signal notify(string message)

    readonly property int matchingPage: 4

    OverviewPage {
        anchors.fill: parent
        active: workspace.currentPage === 0
        onStartMatching: workspace.navigate(workspace.matchingPage)
        onReviewApplicants: workspace.navigate(2)
        onEditProject: { projectsPage.loadActiveOrNothing(); workspace.navigate(1) }
        onCreateProject: { projectsPage.startNew(); workspace.navigate(1) }
        onExplain: studentId => workspace.explain(studentId)
        onNotify: message => workspace.notify(message)
    }
    ProjectsPage {
        id: projectsPage
        anchors.fill: parent
        active: workspace.currentPage === 1
        onOpenMatching: workspace.navigate(workspace.matchingPage)
        onNotify: message => workspace.notify(message)
    }
    ApplicantsPage {
        anchors.fill: parent
        active: workspace.currentPage === 2
        onWhyThisMatch: (studentId, requirementId) => workspace.explainFit(studentId, requirementId)
        onOpenMatching: workspace.navigate(workspace.matchingPage)
        onNotify: message => workspace.notify(message)
    }
    ParticipantsPage {
        anchors.fill: parent
        active: workspace.currentPage === 3
        onExplain: studentId => workspace.explain(studentId)
        onNotify: message => workspace.notify(message)
    }
    MatchingPage {
        anchors.fill: parent
        active: workspace.currentPage === workspace.matchingPage
        explainedId: workspace.explainedId
        onCreateProject: { projectsPage.startNew(); workspace.navigate(1) }
        onExplain: studentId => workspace.explain(studentId)
        onNotify: message => workspace.notify(message)
    }
}

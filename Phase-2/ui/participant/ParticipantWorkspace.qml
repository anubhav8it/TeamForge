import QtQuick
import TeamForge

// Participant workspace: Home, My profile, Opportunities. Without a profile it shows the
// five-step onboarding instead; "Edit profile" reuses the same editor. No host or developer
// controls exist here, and the backend rejects their actions in this workspace anyway.
Item {
    id: workspace

    property int currentPage: 0
    property bool editing: false
    readonly property bool hasProfile: Backend.myProfileId !== ""
    signal navigate(int index)
    signal explainFit(string studentId, string requirementId)
    signal notify(string message)

    function viewOpportunity(requirementId) {
        opportunityDrawer.show(requirementId)
    }
    function whyThisMatch(requirementId) {
        workspace.explainFit(Backend.myProfileId, requirementId)
    }
    function expressInterest(requirementId) {
        if (Backend.expressInterest(requirementId))
            workspace.notify(qsTr("Interest sent. The host will review it."))
    }

    ProfileEditor {
        anchors.fill: parent
        visible: !workspace.hasProfile || workspace.editing
        editing: workspace.hasProfile
        onFinished: created => {
            workspace.editing = false
            workspace.notify(created ? qsTr("Welcome to TeamForge — your profile is ready") : qsTr("Profile saved"))
            workspace.navigate(0)
        }
        onClaimed: {
            workspace.notify(qsTr("Welcome back, %1").arg((Backend.myProfile.name ?? "").split(" ")[0]))
            workspace.navigate(0)
        }
        onCancelled: workspace.editing = false
    }

    Item {
        anchors.fill: parent
        visible: workspace.hasProfile && !workspace.editing

        ParticipantHome {
            anchors.fill: parent
            active: parent.visible && workspace.currentPage === 0
            onEditProfile: workspace.editing = true
            onOpenProfile: workspace.navigate(1)
            onOpenOpportunities: workspace.navigate(2)
            onViewOpportunity: id => workspace.viewOpportunity(id)
            onWhyThisMatch: id => workspace.whyThisMatch(id)
            onExpressInterest: id => workspace.expressInterest(id)
        }
        MyProfilePage {
            anchors.fill: parent
            active: parent.visible && workspace.currentPage === 1
            onEditProfile: workspace.editing = true
            onViewOpportunity: id => workspace.viewOpportunity(id)
            onWhyThisMatch: id => workspace.whyThisMatch(id)
        }
        OpportunitiesPage {
            anchors.fill: parent
            active: parent.visible && workspace.currentPage === 2
            onViewOpportunity: id => workspace.viewOpportunity(id)
            onWhyThisMatch: id => workspace.whyThisMatch(id)
            onExpressInterest: id => workspace.expressInterest(id)
        }
    }

    OpportunityDrawer {
        id: opportunityDrawer
        onWhyThisMatch: id => workspace.whyThisMatch(id)
        onExpressInterest: id => workspace.expressInterest(id)
    }
}

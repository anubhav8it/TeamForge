import QtQuick
import TeamForge

// A team's avatar: a rounded square (people are circles, so a team is never mistaken for a
// member) with the team's initials on a colour derived from its name, the same in every list,
// chat and notification.
Rectangle {
    id: team

    property string name
    property int size: 40

    implicitWidth: size
    implicitHeight: size
    radius: Math.round(size * 0.3)
    readonly property var style: Theme.avatarStyle("team:" + name)
    gradient: Gradient {
        GradientStop { position: 0.0; color: team.style.from }
        GradientStop { position: 1.0; color: team.style.to }
    }
    border.width: 1
    border.color: "#26FFFFFF"
    Accessible.role: Accessible.Graphic
    Accessible.name: qsTr("Team %1").arg(name)

    Rectangle { x: team.radius; y: 1; width: Math.max(0, team.width - team.radius * 2); height: 1; color: "#40FFFFFF" }
    Text {
        anchors.centerIn: parent
        text: Theme.initials(team.name) || "#"
        textFormat: Text.PlainText
        font.family: Theme.fontFamily
        font.pixelSize: Math.max(10, Math.round(team.size * 0.36))
        font.weight: Font.ExtraBold
        color: "#FFFFFF"
        style: Text.Raised
        styleColor: "#33000000"
    }
}

import QtQuick
import TeamForge

// Status badge for applications ("pending", "approved" ...), anonymous requests and team roles.
Chip {
    property string status
    text: Hub.statusLabel(status)
    tone: Hub.statusTone(status)
    Accessible.name: qsTr("Status: %1").arg(text)
}

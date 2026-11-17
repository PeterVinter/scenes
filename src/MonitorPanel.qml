import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// One monitor's workspaces, in the order the monitor map numbers it.
ColumnLayout {
    id: panel
    property int number: 1
    property var monitor: ({})

    width: 250
    spacing: 8

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        Rectangle {
            Layout.preferredWidth: 22
            Layout.preferredHeight: 22
            radius: 11 // A true circle.
            color: panel.monitor.connected ? theme.accent : theme.selection
            Label {
                anchors.centerIn: parent
                text: Number(panel.number).toLocaleString(Qt.locale(), "f", 0)
                font.pixelSize: 12
                font.bold: true
                color: panel.monitor.connected ? theme.accentForeground : theme.foreground
            }
        }
        Label {
            Layout.fillWidth: true
            text: panel.monitor.label
            font.bold: true
            elide: Text.ElideRight
        }
    }

    Label {
        Layout.fillWidth: true
        visible: !panel.monitor.connected
        text: qsTr("Not connected: its workspaces won't be restored.")
        color: theme.secondary
        wrapMode: Text.Wrap
        font.pixelSize: 12
    }

    Repeater {
        model: panel.monitor.workspaces
        WorkspaceCard {
            required property var modelData
            Layout.fillWidth: true
            workspace: modelData
            dimmed: !panel.monitor.connected
        }
    }
}

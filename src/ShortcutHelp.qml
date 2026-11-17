pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Popup {
    id: overlay
    objectName: "helpOverlay"

    readonly property var keys: [
        { key: "Space", action: qsTr("Restore the selected scene") },
        { key: "N", action: qsTr("Save the current desktop as a new scene") },
        { key: "R", action: qsTr("Rename the selected scene") },
        { key: "Delete", action: qsTr("Delete the selected scene") },
        { key: "↑ ↓", action: qsTr("Select the previous or next scene") },
        { key: "Ctrl+Z", action: qsTr("Undo a delete or rename") },
        { key: "Ctrl+Shift+Z", action: qsTr("Redo") },
        { key: "F1", action: qsTr("Open the help") },
        { key: "?", action: qsTr("Show or hide this list of shortcuts") },
        { key: "Q", action: qsTr("Quit") }
    ]

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    // Material's own dim lightens a dark window; always darken what is behind.
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, theme.light ? 0.3 : 0.55) }
    focus: true
    width: Math.min(520, (parent ? parent.width : 520) - 32)
    padding: 20
    Material.roundedScale: theme.radius

    contentItem: ColumnLayout {
        // qmllint disable missing-property
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        // qmllint enable missing-property
        LayoutMirroring.childrenInherit: true
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: qsTr("Keyboard shortcuts")
            font.pixelSize: 18
            font.bold: true
            elide: Text.ElideRight
        }

        Repeater {
            model: overlay.keys
            RowLayout {
                id: entry
                required property var modelData
                Layout.fillWidth: true
                spacing: 12
                Rectangle {
                    Layout.preferredWidth: 104
                    Layout.preferredHeight: 26
                    radius: Math.min(4, theme.radius)
                    color: theme.surface
                    border.color: theme.border
                    Label {
                        anchors.centerIn: parent
                        text: entry.modelData.key
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: entry.modelData.action
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}

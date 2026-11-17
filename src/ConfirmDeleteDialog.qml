import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Delete asks first. Escape, a click outside and the Cancel button all take the
// answer that loses nothing, and Cancel is the focused button.
Dialog {
    id: dialog
    objectName: "deleteDialog"
    property string sceneName: ""

    function openFor(name) {
        sceneName = name
        open()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    // Material's own dim lightens a dark window; always darken what is behind.
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, theme.light ? 0.3 : 0.55) }
    focus: true
    width: Math.min(420, (parent ? parent.width : 420) - 32)
    title: qsTr("Delete scene?")
    Material.roundedScale: theme.radius
    onOpened: cancel.forceActiveFocus()

    contentItem: Label {
        // qmllint disable missing-property
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        // qmllint enable missing-property
        LayoutMirroring.childrenInherit: true
        text: qsTr("“%1” will be deleted. Ctrl+Z brings it back.").arg(dialog.sceneName)
        wrapMode: Text.Wrap
    }

    footer: RowLayout {
        // qmllint disable missing-property
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        // qmllint enable missing-property
        LayoutMirroring.childrenInherit: true
        spacing: 8
        Item { Layout.fillWidth: true; Layout.leftMargin: 12 }
        Button {
            id: cancel
            objectName: "cancelButton"
            text: qsTr("Cancel")
            highlighted: true
            Material.roundedScale: theme.radius
            Material.background: theme.accent
            Material.foreground: theme.accentForeground
            onClicked: dialog.close()
            // Qt Quick buttons answer Space only; Enter on the safe answer should work too.
            Keys.onReturnPressed: dialog.close()
            Keys.onEnterPressed: dialog.close()
        }
        Button {
            text: qsTr("Delete")
            flat: true
            Layout.rightMargin: 12
            Layout.bottomMargin: 8
            Material.roundedScale: theme.radius
            onClicked: {
                dialog.close()
                backend.remove()
            }
        }
    }
}

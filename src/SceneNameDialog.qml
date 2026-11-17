import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Names a new scene or renames one. Its field takes focus when it opens.
Dialog {
    id: dialog
    objectName: "nameDialog"
    property string mode: "save"
    property string problem: ""

    function openFor(newMode, name) {
        mode = newMode
        problem = ""
        field.text = name
        open()
    }

    function accept() {
        const message = mode === "save" ? backend.saveCurrent(field.text) : backend.rename(field.text)
        if (message === "")
            close()
        else
            problem = message
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    // Material's own dim lightens a dark window; always darken what is behind.
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, theme.light ? 0.3 : 0.55) }
    focus: true
    width: Math.min(420, (parent ? parent.width : 420) - 32)
    title: mode === "save" ? qsTr("Save as scene") : qsTr("Rename scene")
    Material.roundedScale: theme.radius
    onOpened: {
        field.forceActiveFocus()
        field.selectAll()
    }

    contentItem: ColumnLayout {
        // qmllint disable missing-property
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        // qmllint enable missing-property
        LayoutMirroring.childrenInherit: true
        spacing: 8

        // The placeholder is the label, which Material floats above the text.
        TextField {
            id: field
            objectName: "nameField"
            Layout.fillWidth: true
            placeholderText: qsTr("Name")
            Material.roundedScale: theme.radius
            selectByMouse: true
            onAccepted: dialog.accept()
            onTextChanged: dialog.problem = ""
        }
        Label {
            Layout.fillWidth: true
            visible: dialog.problem !== ""
            text: dialog.problem
            wrapMode: Text.Wrap
            color: theme.foreground
        }
    }

    footer: RowLayout {
        // qmllint disable missing-property
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        // qmllint enable missing-property
        LayoutMirroring.childrenInherit: true
        spacing: 8
        Item { Layout.fillWidth: true; Layout.leftMargin: 12 }
        Button {
            text: qsTr("Cancel")
            flat: true
            Material.roundedScale: theme.radius
            onClicked: dialog.close()
        }
        Button {
            text: qsTr("Save")
            highlighted: true
            enabled: field.text.trim() !== ""
            Layout.rightMargin: 12
            Layout.bottomMargin: 8
            Material.roundedScale: theme.radius
            Material.background: theme.accent
            Material.foreground: theme.accentForeground
            onClicked: dialog.accept()
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1040
    height: 800
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: qsTr("Scenes")
    color: theme.background

    Material.theme: theme.light ? Material.Light : Material.Dark
    Material.accent: theme.accent
    Material.background: theme.background
    Material.foreground: theme.foreground
    Material.roundedScale: theme.radius

    readonly property bool dialogOpen: nameDialog.visible || deleteDialog.visible || helpDialog.visible
    // Single-key shortcuts are off whenever something that takes typing or a
    // choice is open, so a "q" in the name field is a letter and not a quit.
    readonly property bool keysEnabled: !dialogOpen && !helpOverlay.visible
    readonly property bool sceneSelected: backend.selected !== ""

    function flash(message) {
        if (message === "")
            return
        toastLabel.text = message
        toast.opacity = 1
        toastTimer.restart()
    }

    function restoreSelected() {
        if (win.sceneSelected && !backend.restoring)
            backend.restore()
    }

    function renameSelected() {
        if (win.sceneSelected)
            nameDialog.openFor("rename", backend.selectedName())
    }

    function deleteSelected() {
        if (win.sceneSelected)
            deleteDialog.openFor(backend.selectedName())
    }

    Shortcut { sequence: "Space"; enabled: win.keysEnabled; onActivated: win.restoreSelected() }
    Shortcut { sequence: "N"; enabled: win.keysEnabled; onActivated: nameDialog.openFor("save", "") }
    Shortcut { sequence: "R"; enabled: win.keysEnabled; onActivated: win.renameSelected() }
    Shortcut { sequence: "Delete"; enabled: win.keysEnabled; onActivated: win.deleteSelected() }
    Shortcut { sequence: "Up"; enabled: win.keysEnabled; onActivated: backend.selectOffset(-1) }
    Shortcut { sequence: "Down"; enabled: win.keysEnabled; onActivated: backend.selectOffset(1) }
    // "?" is Shift+/ on most keyboards, so both spellings are bound.
    Shortcut { sequences: ["?", "Shift+?"]; enabled: !win.dialogOpen; onActivated: helpOverlay.visible ? helpOverlay.close() : helpOverlay.open() }
    Shortcut { sequence: "F1"; enabled: !nameDialog.visible && !deleteDialog.visible && !helpOverlay.visible; onActivated: helpDialog.visible ? helpDialog.close() : helpDialog.openTab(0) }
    Shortcut { sequence: "Q"; enabled: win.keysEnabled; onActivated: win.close() }
    Shortcut { sequences: [StandardKey.Undo]; enabled: win.keysEnabled; onActivated: win.flash(backend.undo()) }
    Shortcut { sequences: [StandardKey.Redo]; enabled: win.keysEnabled; onActivated: win.flash(backend.redo()) }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        // qmllint disable missing-property
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        // qmllint enable missing-property
        LayoutMirroring.childrenInherit: true

        RowLayout {
            anchors.fill: parent
            spacing: 0

            SceneSidebar {
                Layout.preferredWidth: Math.min(320, Math.max(240, win.width * 0.28))
                Layout.fillHeight: true
                onHelpRequested: helpDialog.openTab(0)
                onAboutRequested: helpDialog.openTab(1)
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: theme.border
            }

            SceneDetail {
                Layout.fillWidth: true
                Layout.fillHeight: true
                onSaveRequested: nameDialog.openFor("save", "")
                onRenameRequested: win.renameSelected()
                onDeleteRequested: win.deleteSelected()
            }
        }

        Rectangle {
            id: toast
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20
            width: Math.min(parent.width - 32, toastLabel.implicitWidth + 32)
            height: toastLabel.implicitHeight + 20
            radius: theme.radius
            color: theme.foreground
            opacity: 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 150 } }

            Label {
                id: toastLabel
                anchors.centerIn: parent
                width: parent.width - 32
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: theme.background
            }
            Timer { id: toastTimer; interval: 3500; onTriggered: toast.opacity = 0 }
        }
    }

    SceneNameDialog { id: nameDialog }
    ConfirmDeleteDialog { id: deleteDialog }
    ShortcutHelp { id: helpOverlay }
    HelpDialog { id: helpDialog }
}

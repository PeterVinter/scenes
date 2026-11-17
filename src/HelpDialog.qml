import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// What Scenes is and how it works, and who it is by. F1 opens it.
Popup {
    id: dialog
    objectName: "helpDialog"

    function openTab(index) {
        tabs.currentIndex = index
        open()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    // Material's own dim lightens a dark window; always darken what is behind.
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, theme.light ? 0.3 : 0.55) }
    width: Math.min(600, (parent ? parent.width : 600) - 32)
    height: Math.min(560, (parent ? parent.height : 560) - 32)
    padding: 20
    Material.roundedScale: theme.radius

    // Each heading and its text, as one list so the layout stays one loop.
    readonly property var guide: [
        { heading: qsTr("What is a scene?"),
          text: qsTr("A scene is a named setup of your desktop. For every monitor and workspace it keeps the apps that are open and where floating windows sat. Think Coding, Meeting or Writing.") },
        { heading: qsTr("Save a scene"),
          text: qsTr("Arrange your desktop the way you want it and press N. The whole desktop is saved at once: every monitor and every workspace with its apps, not only the one you are looking at. Give it a name and it is saved. Current desktop, at the top of the list, always shows what is open right now.") },
        { heading: qsTr("Restore a scene"),
          text: qsTr("Select a scene and press Space. The whole scene is restored at once, on every monitor and workspace: Scenes opens the apps that are missing, each on its own workspace, and puts floating windows back. It never closes or moves a window that is already open.") },
        { heading: qsTr("Monitors"),
          text: qsTr("A scene is tied to your physical monitors (make, model and serial), not to their connector names. A monitor that isn't connected is drawn dimmed, and its workspaces are not restored. They are never moved to another monitor.") },
        { heading: qsTr("What Scenes can't do"),
          text: qsTr("Layouts are saved and shown but not applied, because Omarchy has no command to set the layout of a specific workspace. Named workspaces are skipped, and so are windows without a desktop entry.") },
        { heading: qsTr("Command line"),
          text: qsTr("Everything is also a command: scenes list, save, restore and check, each with --json. Run scenes --help to see them.") }
    ]

    contentItem: ColumnLayout {
        // qmllint disable missing-property
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        // qmllint enable missing-property
        LayoutMirroring.childrenInherit: true
        spacing: 12

        TabBar {
            id: tabs
            Layout.fillWidth: true
            Material.roundedScale: theme.radius
            TabButton { text: qsTr("Help") }
            TabButton { text: qsTr("About") }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            ScrollView {
                id: guideScroll
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: guideScroll.availableWidth
                    spacing: 14
                    Repeater {
                        model: dialog.guide
                        ColumnLayout {
                            id: section
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 4
                            Label {
                                Layout.fillWidth: true
                                text: section.modelData.heading
                                font.bold: true
                                font.pixelSize: 15
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                text: section.modelData.text
                                color: theme.secondary
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                spacing: 10

                Item { Layout.fillHeight: true }
                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: "image://appicon/scenes"
                    sourceSize: Qt.size(128, 128)
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 64
                    fillMode: Image.PreserveAspectFit
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Scenes")
                    font.pixelSize: 22
                    font.bold: true
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Version %1").arg(backend.version)
                    color: theme.secondary
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Save and restore named setups of your Omarchy desktop.")
                    wrapMode: Text.Wrap
                }
                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Your scenes are kept in %1").arg(backend.sceneDirectory)
                    color: theme.secondary
                    wrapMode: Text.WrapAnywhere
                    font.pixelSize: 12
                }
                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Made by %1").arg("Peter Vinter")
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Released under the MIT License.")
                    color: theme.secondary
                    font.pixelSize: 12
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: root

    signal saveRequested
    signal renameRequested
    signal deleteRequested

    readonly property var view: backend.view
    readonly property bool current: backend.selected === ""
    readonly property var monitors: view.monitors !== undefined ? view.monitors : []
    readonly property var result: backend.result.scene !== undefined && backend.result.scene === backend.selected
                                  ? backend.result : ({})

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    Layout.fillWidth: true
                    text: root.current ? qsTr("Current desktop") : (root.view.name !== undefined ? root.view.name : "")
                    font.pixelSize: 22
                    font.bold: true
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    visible: root.view.apps !== undefined
                    text: root.view.apps === undefined ? "" : (translator.language !== "" ? qsTr("%1, %2, %3")
                          .arg(qsTr("%Ln apps", "", root.view.apps))
                          .arg(qsTr("%Ln workspaces", "", root.view.workspaces))
                          .arg(qsTr("%Ln monitors", "", root.monitors.length)) : "")
                    color: theme.secondary
                    elide: Text.ElideRight
                }
            }

            Button {
                visible: root.current
                text: qsTr("Save as scene")
                highlighted: true
                enabled: root.monitors.length > 0
                Material.roundedScale: theme.radius
                Material.background: theme.accent
                Material.foreground: theme.accentForeground
                onClicked: root.saveRequested()
            }
            Button {
                visible: !root.current
                text: qsTr("Restore")
                highlighted: true
                enabled: !backend.restoring
                Material.roundedScale: theme.radius
                Material.background: theme.accent
                Material.foreground: theme.accentForeground
                onClicked: backend.restore()
            }
            Button {
                visible: !root.current
                text: qsTr("Rename")
                flat: true
                Material.roundedScale: theme.radius
                onClicked: root.renameRequested()
            }
            Button {
                visible: !root.current
                text: qsTr("Delete")
                flat: true
                Material.roundedScale: theme.radius
                onClicked: root.deleteRequested()
            }
        }

        // Progress while a restore runs, then how it went.
        Rectangle {
            Layout.fillWidth: true
            visible: backend.restoring
            implicitHeight: progressColumn.implicitHeight + 24
            color: theme.surface
            border.color: theme.border
            radius: theme.radius
            ColumnLayout {
                id: progressColumn
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    text: backend.progressTotal > 0
                          ? qsTr("Opening %1 (%2 of %3)").arg(backend.progressApp).arg(backend.progressDone + 1).arg(backend.progressTotal)
                          : qsTr("Checking the desktop…")
                    elide: Text.ElideRight
                }
                ProgressBar {
                    Layout.fillWidth: true
                    Material.roundedScale: theme.radius
                    indeterminate: backend.progressTotal === 0
                    value: backend.progressTotal > 0 ? backend.progressDone / backend.progressTotal : 0
                }
            }
        }

        RestoreResult {
            Layout.fillWidth: true
            visible: !backend.restoring && root.result.launched !== undefined
            result: root.result
        }

        // Why the desktop can't be shown, in words, never a blank pane.
        Label {
            Layout.fillWidth: true
            visible: root.current && backend.currentError !== ""
            text: qsTr("The current desktop can't be read: %1").arg(backend.currentError)
            color: theme.foreground
            wrapMode: Text.Wrap
        }

        MonitorMap {
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            monitors: root.monitors
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: panels.implicitWidth
            contentHeight: panels.implicitHeight

            Row {
                id: panels
                spacing: 12
                Repeater {
                    model: root.monitors
                    MonitorPanel {
                        required property int index
                        required property var modelData
                        height: implicitHeight
                        number: index + 1
                        monitor: modelData
                    }
                }
            }
        }

        // What the view leaves out, and what restore can't do.
        Label {
            Layout.fillWidth: true
            visible: root.current && root.view.skippedNamed !== undefined && root.view.skippedNamed.length > 0
            text: root.view.skippedNamed === undefined ? "" : qsTr("%Ln named workspaces are skipped: %1", "", root.view.skippedNamed.length)
                  .arg(root.view.skippedNamed.join(", "))
            color: theme.secondary
            wrapMode: Text.Wrap
            font.pixelSize: 12
        }
        Label {
            Layout.fillWidth: true
            visible: root.current && root.view.unmappedWindows !== undefined && root.view.unmappedWindows > 0
            text: root.view.unmappedWindows === undefined ? "" : qsTr("%Ln windows without a desktop entry are left out: %1", "", root.view.unmappedWindows)
                  .arg(root.view.unmappedClasses.join(", "))
            color: theme.secondary
            wrapMode: Text.Wrap
            font.pixelSize: 12
        }
        Label {
            Layout.fillWidth: true
            visible: !root.current
            text: qsTr("Restore opens the apps that are missing from their workspaces and never closes or moves a window. Layouts are shown but not applied: Omarchy has no command to set the layout of a specific workspace.")
            color: theme.secondary
            wrapMode: Text.Wrap
            font.pixelSize: 12
        }
    }
}

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// How the last restore went. Anything that was not done is said here, in the
// window, and not worked around.
Rectangle {
    id: box
    property var result: ({})
    // Numbers for the counted strings: qsTr() rejects undefined while no restore has run.
    readonly property int launched: Number(result.launched) || 0
    readonly property int alreadyRunning: Number(result.alreadyRunning) || 0

    implicitHeight: lines.implicitHeight + 24
    color: theme.surface
    border.color: theme.border
    radius: theme.radius

    function layoutName(layout) {
        if (layout === "dwindle") return qsTr("Tiling")
        if (layout === "scrolling") return qsTr("Scrolling")
        if (layout === "floating") return qsTr("Floating")
        return layout
    }

    ColumnLayout {
        id: lines
        anchors.fill: parent
        anchors.margins: 12
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: translator.language === "" ? "" : (box.launched > 0
                      ? qsTr("Opened %Ln apps.", "", box.launched)
                      : qsTr("Nothing to open: every app is already where the scene wants it."))
                font.bold: true
                wrapMode: Text.Wrap
            }
            Button {
                text: qsTr("Dismiss")
                flat: true
                Material.roundedScale: theme.radius
                onClicked: backend.dismissResult()
            }
        }

        Label {
            Layout.fillWidth: true
            visible: box.launched > 0 && box.alreadyRunning > 0
            text: qsTr("%Ln were already open.", "", box.alreadyRunning)
            color: theme.secondary
            wrapMode: Text.Wrap
        }

        Repeater {
            model: box.result.failed !== undefined ? box.result.failed : []
            Label {
                id: failure
                required property string modelData
                Layout.fillWidth: true
                text: qsTr("Couldn't start %1").arg(failure.modelData)
                wrapMode: Text.Wrap
            }
        }

        Repeater {
            model: box.result.unplaced !== undefined ? box.result.unplaced : []
            Label {
                id: gone
                required property var modelData
                Layout.fillWidth: true
                text: qsTr("%1 isn't connected: its %Ln workspaces were not placed.", "", gone.modelData.workspaces).arg(gone.modelData.label)
                wrapMode: Text.Wrap
            }
        }

        Label {
            Layout.fillWidth: true
            visible: box.result.missing !== undefined && box.result.missing.length > 0
            text: box.result.missing === undefined ? "" : qsTr("Not installed here: %1").arg(box.result.missing.join(", "))
            wrapMode: Text.Wrap
        }

        Repeater {
            model: box.result.layouts !== undefined ? box.result.layouts : []
            Label {
                id: gap
                required property var modelData
                Layout.fillWidth: true
                text: qsTr("Layout not applied on workspace %1: the scene has %2, it is %3.")
                      .arg(Number(gap.modelData.workspace).toLocaleString(Qt.locale(), "f", 0))
                      .arg(box.layoutName(gap.modelData.wanted)).arg(box.layoutName(gap.modelData.current))
                color: theme.secondary
                wrapMode: Text.Wrap
                font.pixelSize: 12
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: card
    property var workspace: ({ slot: 1, layout: "dwindle", apps: [] })
    property bool dimmed: false

    implicitHeight: content.implicitHeight + 20
    color: theme.surface
    border.color: theme.border
    radius: theme.radius
    opacity: dimmed ? 0.65 : 1

    // Omarchy's menu and bar call dwindle "tiling".
    function layoutName(layout) {
        if (layout === "dwindle") return qsTr("Tiling")
        if (layout === "scrolling") return qsTr("Scrolling")
        if (layout === "floating") return qsTr("Floating")
        return layout
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: qsTr("Workspace %1").arg(Number(card.workspace.slot).toLocaleString(Qt.locale(), "f", 0))
                font.bold: true
                elide: Text.ElideRight
            }
            Label {
                text: card.layoutName(card.workspace.layout)
                color: theme.secondary
                font.pixelSize: 12
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: card.workspace.apps
                Image {
                    required property var modelData
                    width: 28
                    height: 28
                    source: "image://appicon/" + modelData.id
                    sourceSize: Qt.size(56, 56)
                    fillMode: Image.PreserveAspectFit
                    opacity: modelData.installed ? 1 : 0.4
                    HoverHandler { id: iconHover }
                    ToolTip.visible: iconHover.hovered
                    ToolTip.text: modelData.installed ? modelData.name : qsTr("%1 (not installed)").arg(modelData.name)
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: card.workspace.apps.length === 0
            text: qsTr("Empty")
            color: theme.secondary
            font.pixelSize: 12
        }
    }
}

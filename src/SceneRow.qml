import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: row
    property string title
    property string subtitle
    property bool selected: false
    signal clicked

    implicitHeight: 54
    color: selected ? theme.selection : (hover.hovered ? Qt.alpha(theme.selection, 0.5) : "transparent")
    radius: Math.min(4, theme.radius)

    // The accent bar marks the selection on the leading edge, in either text direction.
    Rectangle {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: 3
        height: parent.height - 16
        radius: Math.min(2, theme.radius)
        color: theme.accent
        visible: row.selected
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 2

        Item { Layout.fillHeight: true }
        Label {
            Layout.fillWidth: true
            text: row.title
            font.bold: row.selected
            elide: Text.ElideRight
        }
        Label {
            Layout.fillWidth: true
            text: row.subtitle
            color: theme.secondary
            font.pixelSize: 12
            elide: Text.ElideRight
        }
        Item { Layout.fillHeight: true }
    }

    HoverHandler { id: hover }
    TapHandler { onTapped: row.clicked() }
}

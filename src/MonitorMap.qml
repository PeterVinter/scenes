import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// The monitors as rectangles in their real arrangement, scaled to fit. The
// physical arrangement stays as it is in a right-to-left language: a monitor
// on the left of the desk is still on the left.
Item {
    id: root
    property var monitors: []

    readonly property int pad: 8
    readonly property var bounds: {
        var x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9
        for (var i = 0; i < monitors.length; i++) {
            var m = monitors[i]
            x0 = Math.min(x0, m.x)
            y0 = Math.min(y0, m.y)
            x1 = Math.max(x1, m.x + Math.max(1, m.width))
            y1 = Math.max(y1, m.y + Math.max(1, m.height))
        }
        return monitors.length > 0 ? { x: x0, y: y0, w: x1 - x0, h: y1 - y0 } : { x: 0, y: 0, w: 1, h: 1 }
    }
    readonly property real fit: Math.max(0.001, Math.min((width - 2 * pad) / bounds.w, (height - 2 * pad) / bounds.h))
    readonly property real offsetX: (width - bounds.w * fit) / 2
    readonly property real offsetY: (height - bounds.h * fit) / 2

    Repeater {
        model: root.monitors
        Rectangle {
            required property int index
            required property var modelData
            x: root.offsetX + (modelData.x - root.bounds.x) * root.fit
            y: root.offsetY + (modelData.y - root.bounds.y) * root.fit
            width: Math.max(1, modelData.width) * root.fit - 4
            height: Math.max(1, modelData.height) * root.fit - 4
            radius: Math.min(4, theme.radius)
            color: theme.surface
            border.width: 2
            border.color: modelData.connected ? theme.accent : theme.border
            opacity: modelData.connected ? 1 : 0.7

            Rectangle {
                id: badge
                x: 8
                y: 8
                width: 22
                height: 22
                radius: 11 // A true circle.
                color: modelData.connected ? theme.accent : theme.selection
                Label {
                    anchors.centerIn: parent
                    text: Number(parent.parent.index + 1).toLocaleString(Qt.locale(), "f", 0)
                    font.pixelSize: 12
                    font.bold: true
                    color: modelData.connected ? theme.accentForeground : theme.foreground
                }
            }
            Column {
                anchors.left: badge.right
                anchors.leftMargin: 8
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: badge.verticalCenter
                Label {
                    width: parent.width
                    text: modelData.label
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }
            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 8
                visible: !modelData.connected
                text: qsTr("Not connected")
                font.pixelSize: 12
                color: theme.secondary
                elide: Text.ElideRight
            }
        }
    }
}

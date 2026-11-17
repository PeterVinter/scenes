import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root
    color: theme.surface

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Label {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.bottomMargin: 8
            text: qsTr("Scenes")
            font.pixelSize: 20
            font.bold: true
            elide: Text.ElideRight
        }

        // The live desktop is always first, and can't be deleted or renamed.
        SceneRow {
            Layout.fillWidth: true
            title: qsTr("Current desktop")
            subtitle: qsTr("What is open right now")
            selected: backend.selected === ""
            onClicked: backend.select("")
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            Layout.topMargin: 4
            Layout.bottomMargin: 4
            color: theme.border
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: backend.scenes
            visible: count > 0
            boundsBehavior: Flickable.StopAtBounds
            delegate: SceneRow {
                required property var modelData
                width: ListView.view.width
                title: modelData.name
                // Reading translator.language makes the plural forms re-render on a language switch.
                subtitle: translator.language !== "" ? qsTr("%1, %2").arg(qsTr("%Ln apps", "", modelData.apps))
                                                          .arg(qsTr("%Ln workspaces", "", modelData.workspaces)) : ""
                selected: backend.selected === modelData.slug
                onClicked: backend.select(modelData.slug)
            }
        }

        // The empty state explains how to save the first scene.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 16
            visible: list.count === 0
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: qsTr("No scenes yet")
                font.bold: true
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Arrange your desktop the way you want it, then press N to save it as a scene.")
                color: theme.secondary
                wrapMode: Text.Wrap
            }
            Item { Layout.fillHeight: true }
        }

        Label {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.topMargin: 8
            visible: backend.sceneProblems.length > 0
            text: qsTr("%Ln scene files couldn't be read: %1", "", backend.sceneProblems.length).arg(backend.sceneProblems.join("; "))
            color: theme.secondary
            wrapMode: Text.Wrap
            font.pixelSize: 12
        }

        Label {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.topMargin: 8
            text: qsTr("Press ? for shortcuts")
            color: theme.secondary
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CustomComponents
import Librum.style
import Librum.fonts
import Librum.lute 1.0

/***
 * Fork addition: Lute tag management (tags table). Tags can be attached to
 * terms; the assignment UI follows with the term editor.
 */
Page {
    id: root
    horizontalPadding: 44
    bottomPadding: 22
    background: Rectangle {
        anchors.fill: parent
        color: Style.colorPageBackground
    }

    property int editingId: 0

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: root.horizontalPadding
        anchors.rightMargin: root.horizontalPadding
        anchors.bottomMargin: root.bottomPadding
        spacing: 0

        MTitle {
            Layout.topMargin: 40
            titleText: qsTr("Tags")
            descriptionText: qsTr("%1 tags in lute.db").arg(LuteController.tags.length)
            titleSize: Fonts.size25
            descriptionSize: Fonts.size13dot25
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 20
            spacing: 8

            Pane {
                Layout.preferredWidth: 260
                Layout.preferredHeight: 34
                padding: 0
                background: Rectangle {
                    color: Style.colorContainerBackground
                    border.width: 1
                    border.color: Style.colorContainerBorder
                    radius: 5
                }

                TextField {
                    id: nameInput
                    anchors.fill: parent
                    leftPadding: 12
                    verticalAlignment: TextInput.AlignVCenter
                    background: Item {}
                    color: Style.colorBaseInputText
                    placeholderText: qsTr("New tag…")
                    placeholderTextColor: Style.colorPlaceholderText
                    font.pointSize: Fonts.size11
                    selectByMouse: true
                }
            }

            Pane {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                padding: 0
                background: Rectangle {
                    color: Style.colorContainerBackground
                    border.width: 1
                    border.color: Style.colorContainerBorder
                    radius: 5
                }

                TextField {
                    id: commentInput
                    anchors.fill: parent
                    leftPadding: 12
                    verticalAlignment: TextInput.AlignVCenter
                    background: Item {}
                    color: Style.colorBaseInputText
                    placeholderText: qsTr("Comment (optional)")
                    placeholderTextColor: Style.colorPlaceholderText
                    font.pointSize: Fonts.size11
                    selectByMouse: true
                }
            }

            MButton {
                Layout.preferredHeight: 34
                horizontalMargins: 14
                borderWidth: 0
                backgroundColor: Style.colorBasePurple
                textColor: Style.colorFocusedButtonText
                fontSize: Fonts.size11
                text: qsTr("Add tag")

                onClicked: {
                    if (LuteController.saveTag(root.editingId, nameInput.text,
                                               commentInput.text)) {
                        nameInput.text = ""
                        commentInput.text = ""
                        root.editingId = 0
                    }
                }
            }
        }

        ListView {
            id: tagList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 16
            clip: true
            spacing: 6
            model: LuteController.tags

            delegate: Pane {
                id: tagRow
                required property var modelData

                width: tagList.width
                height: 50
                padding: 0
                background: Rectangle {
                    color: Style.colorContainerBackground
                    border.width: 1
                    border.color: Style.colorContainerBorder
                    radius: 4
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 12

                    Label {
                        Layout.preferredWidth: 220
                        text: tagRow.modelData.text
                        color: Style.colorText
                        font.pointSize: Fonts.size12
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: tagRow.modelData.comment
                        color: Style.colorPageSubtitle
                        font.pointSize: Fonts.size10
                        elide: Text.ElideRight
                    }

                    MButton {
                        Layout.preferredHeight: 28
                        horizontalMargins: 10
                        borderWidth: 1
                        borderColor: Style.colorErrorBorder
                        backgroundColor: "transparent"
                        textColor: Style.colorErrorText
                        fontSize: Fonts.size9
                        text: qsTr("Delete")

                        onClicked: LuteController.deleteTag(tagRow.modelData.id)
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: LuteController.tags.length === 0
                text: qsTr("No tags yet.")
                color: Style.colorPageSubtitle
                font.pointSize: Fonts.size11
            }
        }
    }

    Component.onCompleted: LuteController.refreshTags()
}

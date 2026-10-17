import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CustomComponents
import Librum.style
import Librum.fonts
import Librum.lute 1.0

/***
 * Fork addition: Lute statistics in Librum's look. Reuses the numbers of the
 * controller (status counts, words read) and the 14-day activity series from
 * lute.db (status changes and new terms).
 */
Page {
    id: root
    horizontalPadding: 44
    bottomPadding: 22
    background: Rectangle {
        anchors.fill: parent
        color: Style.colorPageBackground
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: root.horizontalPadding
        anchors.rightMargin: root.horizontalPadding
        anchors.bottomMargin: root.bottomPadding
        spacing: 0

        RowLayout {
            Layout.fillWidth: true

            MTitle {
                Layout.topMargin: 40
                titleText: qsTr("Statistics")
                descriptionText: qsTr("%1 · last 14 days").arg(LuteController.languageName)
                titleSize: Fonts.size25
                descriptionSize: Fonts.size13dot25
            }

            Item { Layout.fillWidth: true }

            MButton {
                Layout.preferredHeight: 34
                Layout.alignment: Qt.AlignBottom
                horizontalMargins: 12
                borderWidth: 1
                borderColor: Style.colorButtonBorder
                backgroundColor: "transparent"
                textColor: Style.colorText
                fontSize: Fonts.size10
                text: qsTr("Refresh")

                onClicked: LuteController.refreshStats()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 22
            spacing: 14

            component StatCard: Pane {
                id: card
                property string label
                property int value
                property color accent: Style.colorText

                Layout.fillWidth: true
                Layout.preferredHeight: 96
                verticalPadding: 18
                horizontalPadding: 20
                background: Rectangle {
                    color: Style.colorContainerBackground
                    border.width: 1
                    border.color: Style.colorContainerBorder
                    radius: 4
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    Label {
                        text: card.value
                        color: card.accent
                        font.pointSize: Fonts.size22
                        font.weight: Font.DemiBold
                    }

                    Label {
                        text: card.label
                        color: Style.colorPageSubtitle
                        font.pointSize: Fonts.size10
                    }
                }
            }

            StatCard { label: qsTr("Terms"); value: LuteController.stats.totalTerms }
            StatCard {
                label: qsTr("Learning")
                value: LuteController.stats.learningTerms
                accent: Style.colorBasePurple
            }
            StatCard {
                label: qsTr("Known")
                value: LuteController.stats.knownTerms
                accent: "#A8DDB5"
            }
            StatCard { label: qsTr("Words read"); value: LuteController.stats.wordsReadTotal }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 16
            spacing: 14

            component SeriesCard: Pane {
                id: seriesCard
                property string title
                property var series
                property color barColor

                Layout.fillWidth: true
                Layout.preferredHeight: 220
                verticalPadding: 18
                horizontalPadding: 18
                background: Rectangle {
                    color: Style.colorContainerBackground
                    border.width: 1
                    border.color: Style.colorContainerBorder
                    radius: 4
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    Label {
                        text: seriesCard.title
                        color: Style.colorText
                        font.pointSize: Fonts.size13
                        font.weight: Font.DemiBold
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        RowLayout {
                            anchors.fill: parent
                            spacing: 4

                            Repeater {
                                model: seriesCard.series

                                delegate: Rectangle {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.maximumWidth: 26
                                    height: Math.max(2, 120 * ((modelData && modelData.count)
                                                              ? modelData.count : 0)
                                                     / Math.max(1, seriesCard.maxCount()))
                                    Layout.alignment: Qt.AlignBottom
                                    radius: 2
                                    color: seriesCard.barColor
                                    opacity: modelData.count > 0 ? 1 : 0.25
                                }
                            }
                        }
                    }
                }

                function maxCount() {
                    let max = 1
                    for (let i = 0; i < series.length; ++i)
                        max = Math.max(max, series[i].count)
                    return max
                }
            }

            SeriesCard {
                title: qsTr("Terms created")
                series: LuteController.createdSeries
                barColor: Style.colorBasePurple
            }

            SeriesCard {
                title: qsTr("Status changes")
                series: LuteController.learnedSeries
                barColor: "#8FBF9F"
            }
        }

        Pane {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 16
            Layout.bottomMargin: 16
            verticalPadding: 18
            horizontalPadding: 20
            background: Rectangle {
                color: Style.colorContainerBackground
                border.width: 1
                border.color: Style.colorContainerBorder
                radius: 4
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                Label {
                    text: qsTr("Status distribution")
                    color: Style.colorText
                    font.pointSize: Fonts.size13
                    font.weight: Font.DemiBold
                }

                Repeater {
                    model: LuteController.stats.byStatus

                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            Layout.preferredWidth: 12
                            Layout.preferredHeight: 12
                            radius: 2
                            color: modelData.color
                        }

                        Label {
                            Layout.fillWidth: true
                            text: modelData.label
                            color: Style.colorText
                            font.pointSize: Fonts.size11
                        }

                        Label {
                            text: modelData.count
                            color: Style.colorPageSubtitle
                            font.pointSize: Fonts.size11
                            font.weight: Font.DemiBold
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }

    Component.onCompleted: LuteController.refreshStats()
}

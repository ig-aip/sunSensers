import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    anchors.fill: parent
    color: "#f0f0f0"

    property int currentIndex: -1

    RowLayout {
        anchors.fill: parent

        // Левая колонка: список датчиков
        Rectangle {
            width: 220
            color: "#eaeaea"
            Layout.fillHeight: true

            Column {
                anchors.fill: parent
                spacing: 6
                padding: 8

                Text { text: "Sensors"; font.bold: true; font.pointSize: 14 }

                ListView {
                    id: sensorList
                    model: sensorModel
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.top: parent.top
                    spacing: 4
                    clip: true
                    delegate: Loader { sourceComponent: sensorDelegate }
                }
            }
        }

        // Центральная область
        Rectangle {
            color: "white"
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 4
            anchors.margins: 8
            border.color: "#cccccc"
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Row {
                    width: parent.width
                    spacing: 10
                    Text {
                        id: title
                        text: currentIndex >= 0 ? sensorModel.get(currentIndex).sensorName : "No sensor selected"
                        font.pointSize: 16
                        font.bold: true
                    }
                    Rectangle { width: 1; height: 24; color: "transparent"; Layout.fillWidth: true }
                    Button {
                        text: "Generate Report"
                        enabled: currentIndex >= 0
                        onClicked: {
                            // Вызываем C++ слот: сохранит CSV в Documents
                            sensorModel.generateReport(currentIndex)
                        }
                    }
                }

                // Таблица данных датчика
                ListView {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: title.bottom
                    anchors.bottom: parent.bottom
                    model: currentIndex >= 0 ? sensorModel.get(currentIndex).sensorData : []
                    spacing: 2
                    delegate: Rectangle {
                        width: parent.width
                        height: 32
                        color: index % 2 === 0 ? "#ffffff" : "#f7f7f7"
                        Row {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 12
                            Text { text: "t: " + (model.time !== undefined ? model.time : "") ; width: 120 }
                            Text { text: "v1: " + (model.v1 !== undefined ? model.v1 : "") ; width: 120 }
                            Text { text: "v2: " + (model.v2 !== undefined ? model.v2 : "") ; width: 120 }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: sensorDelegate
        Rectangle {
            width: parent.width
            height: 44
            radius: 6
            color: ListView.isCurrentItem ? "#d0eaff" : "#ffffff"
            border.width: 1
            border.color: "#cccccc"
            anchors.margins: 2
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    sensorList.currentIndex = index
                    root.currentIndex = index
                }
            }
            Row {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8
                Text { text: model.sensorName; elide: Text.ElideRight }
            }
        }
    }
}

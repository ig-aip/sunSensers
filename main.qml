import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCharts
import Qt.labs.platform as Platform

Rectangle {
    id: root
    anchors.fill: parent
    color: "#f5f5f5"

    property int currentIndex: -1
    property string viewMode: "raw"
    // Храним данные статистики
    property var currentStats: null

    function getSensorColor(idx) {
        if (idx < 0) return "black";
        return Qt.hsla((idx * 0.618033988749895) % 1.0, 0.75, 0.5, 1.0);
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // МЕНЮ (Верх)
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 35; color: "#343a40"; z: 100
            Row {
                anchors.fill: parent; anchors.leftMargin: 10; spacing: 10
                Button {
                    text: "Файл"
                    background: Rectangle { color: parent.down ? "#555" : "transparent" }
                    contentItem: Text { text: parent.text; color: "white"; font.bold: true; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter }
                    onClicked: fileMenu.open()
                    Menu { id: fileMenu; y: parent.height
                        MenuItem { text: "Импорт (.txt)"; onTriggered: openDialog.open() }
                        MenuItem { text: "Экспорт (.csv)"; onTriggered: saveDialog.open() }
                        MenuItem { text: "Экспорт JSON (с коэфф.)"; onTriggered: saveJsonDialog.open() }
                    }
                }
                Button {
                    text: "Вид"
                    background: Rectangle { color: parent.down ? "#555" : "transparent" }
                    contentItem: Text { text: parent.text; color: "white"; font.bold: true; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter }
                    onClicked: viewMenu.open()
                    Menu { id: viewMenu; y: parent.height
                        MenuItem { text: "Сырые данные"; checkable: true; checked: root.viewMode==="raw"; onTriggered: { root.viewMode="raw"; updateChart() } }
                        MenuItem { text: "Скорректированные"; checkable: true; checked: root.viewMode==="corrected"; onTriggered: { root.viewMode="corrected"; updateChart() } }
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter; leftPadding: 20
                    text: root.viewMode === "raw" ? "РЕЖИМ: СЫРЫЕ" : "РЕЖИМ: КОРРЕКЦИЯ"
                    color: root.viewMode === "raw" ? "#ffc107" : "#28a745"; font.bold: true
                }
            }
        }

        // РАБОЧАЯ ОБЛАСТЬ
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0

            // 1. ЛЕВАЯ ПАНЕЛЬ (Список)
            Rectangle {
                Layout.preferredWidth: 260; Layout.fillHeight: true; color: "white"; border.color: "#ddd"
                ColumnLayout {
                    anchors.fill: parent; spacing: 0
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 40; color: root.currentIndex===-1?"#007bff":"#f8f9fa"
                        Text { anchors.centerIn: parent; text: "Все датчики"; color: root.currentIndex===-1?"white":"black"; font.bold: true }
                        MouseArea { anchors.fill: parent; onClicked: { root.currentIndex = -1; updateChart() } }
                    }
                    ListView {
                        id: sensorList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; model: sensorModel; spacing: 1
                        delegate: Rectangle {
                            width: ListView.view.width; height: 40; color: root.currentIndex===index?"#e7f1ff":"white"
                            RowLayout { anchors.fill: parent; anchors.leftMargin: 10
                                Rectangle { width: 10; height: 10; radius: 5; color: getSensorColor(index) }
                                Text { text: sensorName; font.bold: root.currentIndex===index }
                            }
                            MouseArea { anchors.fill: parent; onClicked: { root.currentIndex=index; updateChart() } }
                        }
                    }
                }
            }

            // 2. ЦЕНТРАЛЬНАЯ ПАНЕЛЬ (График)
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true; color: "white"; clip: true
                ChartView {
                    id: chart; anchors.fill: parent; antialiasing: true;
                    animationOptions: ChartView.NoAnimation;
                    legend.alignment: Qt.AlignBottom

                    ValueAxis {
                        id: axisX
                        titleText: "Время (с)"
                        labelFormat: "%.1f"
                        min: sensorModel.minTime
                        max: sensorModel.maxTime
                        tickCount: 10
                    }
                    ValueAxis {
                        id: axisY
                        titleText: "Значение"
                        labelFormat: "%.0f"
                        min: sensorModel.minValue
                        max: sensorModel.maxValue
                        tickCount: 5
                    }
                }
            }

            // 3. ПРАВАЯ ПАНЕЛЬ (Статистика)
            Rectangle {
                Layout.preferredWidth: 240; Layout.fillHeight: true
                color: "#f8f9fa"; border.color: "#ddd"

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 15; spacing: 10

                    Text {
                        text: "Информация"; font.pointSize: 14; font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#ccc" }

                    // Данные
                    Item {
                        Layout.fillWidth: true; Layout.fillHeight: true

                        ColumnLayout {
                            anchors.fill: parent
                            visible: root.currentStats !== null
                            spacing: 15

                            // --- Режим: ВСЕ СЕНСОРЫ ---
                            ColumnLayout {
                                visible: root.currentStats && root.currentStats.type === "all"
                                spacing: 5
                                Text { text: "Режим: Общий обзор"; font.bold: true; color: "#555" }

                                Item { height: 10; width: 1 }
                                Text { text: "Глобальный эталон:"; color: "#555" }
                                Text {
                                    // БЕЗОПАСНАЯ ПРОВЕРКА
                                    text: (root.currentStats && root.currentStats.reference !== undefined) ? root.currentStats.reference.toFixed(2) : "0"
                                    font.pointSize: 14; font.bold: true; color: "#007bff"
                                }

                                Item { height: 10; width: 1 }
                                Text { text: "Средняя коррекция:"; color: "#555" }
                                Text {
                                    // БЕЗОПАСНАЯ ПРОВЕРКА
                                    text: (root.currentStats && root.currentStats.avgCorrection !== undefined) ? (root.currentStats.avgCorrection * 100).toFixed(2) + "%" : "0%"
                                    font.pointSize: 14; font.bold: true; color: "#28a745"
                                }
                                Text {
                                    text: "(отклонение от нормы)";
                                    font.pixelSize: 10; color: "#888"; wrapMode: Text.WordWrap; Layout.fillWidth: true
                                }
                            }

                            // --- Режим: ОДИН СЕНСОР ---
                            ColumnLayout {
                                visible: root.currentStats && root.currentStats.type === "single"
                                spacing: 8
                                Text { text: "Выбран сенсор:"; font.bold: true; color: "#555" }
                                Text { text: root.currentStats ? root.currentStats.name : ""; font.pointSize: 12; font.bold: true }

                                Rectangle { Layout.fillWidth: true; height: 1; color: "#eee" }

                                // Канал A
                                Text { text: "Канал A (Сплошной):"; color: getSensorColor(root.currentIndex); font.bold: true }
                                RowLayout {
                                    Text { text: "Коэфф:"; color: "#555" }
                                    Text {
                                        // БЕЗОПАСНАЯ ПРОВЕРКА
                                        text: (root.currentStats && root.currentStats.kA !== undefined) ? "x" + root.currentStats.kA.toFixed(4) : ""
                                        font.bold: true
                                    }
                                }
                                RowLayout {
                                    Text { text: "Погрешность:"; color: "#555" }
                                    Text {
                                        // БЕЗОПАСНАЯ ПРОВЕРКА
                                        text: (root.currentStats && root.currentStats.pA !== undefined) ? root.currentStats.pA.toFixed(0) + "%" : ""
                                        font.bold: true
                                    }
                                }
                                Text {
                                    text: "Среднее сырое: " + ((root.currentStats && root.currentStats.rawA !== undefined) ? root.currentStats.rawA.toFixed(1) : "")
                                    font.pixelSize: 11; color: "#666"
                                }

                                Item { height: 8; width: 1 }

                                // Канал B
                                Text { text: "Канал B (Пунктир):"; color: Qt.lighter(getSensorColor(root.currentIndex), 1.5); font.bold: true }
                                RowLayout {
                                    Text { text: "Коэфф:"; color: "#555" }
                                    Text {
                                        // БЕЗОПАСНАЯ ПРОВЕРКА
                                        text: (root.currentStats && root.currentStats.kB !== undefined) ? "x" + root.currentStats.kB.toFixed(4) : ""
                                        font.bold: true
                                    }
                                }
                                RowLayout {
                                    Text { text: "Погрешность:"; color: "#555" }
                                    Text {
                                        // БЕЗОПАСНАЯ ПРОВЕРКА
                                        text: (root.currentStats && root.currentStats.pB !== undefined) ? root.currentStats.pB.toFixed(0) + "%" : ""
                                        font.bold: true
                                    }
                                }
                                Text {
                                    text: "Среднее сырое: " + ((root.currentStats && root.currentStats.rawB !== undefined) ? root.currentStats.rawB.toFixed(1) : "")
                                    font.pixelSize: 11; color: "#666"
                                }
                            }
                            Item { Layout.fillHeight: true }
                        }
                    }
                }
            }
        }
    }

    Platform.FileDialog { id: openDialog; nameFilters: ["Text (*.txt)"]; onAccepted: { sensorModel.importFromTxt(file.toString()); updateChart(); } }
    Platform.FileDialog { id: saveDialog; fileMode: Platform.FileDialog.SaveFile; nameFilters: ["CSV (*.csv)"]; onAccepted: { sensorModel.exportToCsv(file.toString()); } }
    Platform.FileDialog {
        id: saveJsonDialog
        fileMode: Platform.FileDialog.SaveFile
        nameFilters: ["JSON (*.json)"]
        onAccepted: {
            // Вызов метода C++
            sensorModel.exportToJson(file.toString());
        }
    }

    function updateChart() {
        chart.removeAllSeries();
        var isCorrected = (root.viewMode === "corrected");

        if(currentIndex === -1) {
            var count = sensorModel.rowCount();
            for(var i=0; i < count; i++) {
                var idx = sensorModel.index(i,0);
                var sName = sensorModel.data(idx, 258); // NameRole

                var s = chart.createSeries(ChartView.SeriesTypeLine, sName, axisX, axisY);
                s.color = getSensorColor(i);
                s.width = 2;
                sensorModel.fillSeries(s, i, isCorrected, "A");
            }
        } else {
            var sA = chart.createSeries(ChartView.SeriesTypeLine, "Канал A", axisX, axisY);
            sA.color = getSensorColor(currentIndex);
            sA.width = 3;
            sensorModel.fillSeries(sA, currentIndex, isCorrected, "A");

            var sB = chart.createSeries(ChartView.SeriesTypeLine, "Канал B", axisX, axisY);
            sB.color = Qt.lighter(sA.color, 1.5);
            sB.width = 3;
            sB.style = Qt.DashLine;
            sensorModel.fillSeries(sB, currentIndex, isCorrected, "B");
        }

        // Запрашиваем статистику для правой панели
        root.currentStats = sensorModel.getSensorStats(currentIndex);
    }

    Component.onCompleted: updateTimer.start()
    Timer { id: updateTimer; interval: 200; onTriggered: updateChart() }
}

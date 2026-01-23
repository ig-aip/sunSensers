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

    function getSensorColor(idx) {
        if (idx < 0) return "black";
        return Qt.hsla((idx * 0.618033988749895) % 1.0, 0.75, 0.5, 1.0);
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // МЕНЮ
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

            // Левая панель
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
                            width: parent.width; height: 40; color: root.currentIndex===index?"#e7f1ff":"white"
                            RowLayout { anchors.fill: parent; anchors.leftMargin: 10
                                Rectangle { width: 10; height: 10; radius: 5; color: getSensorColor(index) }
                                Text { text: sensorName; font.bold: root.currentIndex===index }
                            }
                            MouseArea { anchors.fill: parent; onClicked: { root.currentIndex=index; updateChart() } }
                        }
                    }
                }
            }

            // График
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true; color: "white"; clip: true
                ChartView {
                    id: chart; anchors.fill: parent; antialiasing: true;
                    animationOptions: ChartView.NoAnimation;
                    legend.alignment: Qt.AlignBottom

                    // Оси
                    ValueAxis {
                        id: axisX
                        titleText: "Время (с)"
                        min: 0
                        max: 10 // Начальное значение, изменится программно
                    }
                    ValueAxis {
                        id: axisY
                        titleText: "Значение"
                        min: 0
                        max: 100 // Начальное значение, изменится программно
                    }

                    function updateChart() {
                        chart.removeAllSeries();
                        var isCorrected = (root.viewMode === "corrected");

                        var minX = 999999, maxX = -999999;
                        var minY = 999999, maxY = -999999;

                        if(currentIndex === -1) {
                            var count = sensorModel.rowCount();
                            for(var i=0; i < count; i++) {
                                var s = chart.createSeries(ChartView.SeriesTypeLine, "Датчик " + (i+1), axisX, axisY);
                                s.color = getSensorColor(i);
                                s.width = 2;
                                sensorModel.fillSeries(s, i, isCorrected, "A");
                            }
                        } else {
                            var sA = chart.createSeries(ChartView.SeriesTypeLine, "Канал A", axisX, axisY);
                            sA.color = getSensorColor(currentIndex);
                            sensorModel.fillSeries(sA, currentIndex, isCorrected, "A");

                            var sB = chart.createSeries(ChartView.SeriesTypeLine, "Канал B", axisX, axisY);
                            sB.color = Qt.lighter(sA.color, 1.5);
                            sB.style = Qt.DashLine;
                            sensorModel.fillSeries(sB, currentIndex, isCorrected, "B");
                        }

                        // ВАЖНО: Автоматическое масштабирование осей под добавленные данные
                        axisX.applyNiceNumbers();
                        axisY.applyNiceNumbers();

                        // Если автоматика не сработала, принудительно масштабируем по всем сериям:
                        if (chart.count > 0) {
                            chart.zoomReset(); // Сброс зума, чтобы увидеть все данные
                        }
                    }
                }

                // Tooltip пришлось убрать в C++ версии, так как series создаются динамически,
                // но можно вернуть, подключив сигналы внутри createSeries
            }
        }
    }

    Platform.FileDialog { id: openDialog; nameFilters: ["Text (*.txt)"]; onAccepted: { sensorModel.importFromTxt(file.toString()); updateChart(); } }
    Platform.FileDialog { id: saveDialog; fileMode: Platform.FileDialog.SaveFile; nameFilters: ["CSV (*.csv)"]; onAccepted: { sensorModel.exportToCsv(file.toString()); } }

    function updateChart() {
        chart.removeAllSeries();
        var isCorrected = (root.viewMode === "corrected");

        // --- РЕЖИМ: ВСЕ ДАТЧИКИ ---
        if(currentIndex === -1) {
            var count = sensorModel.rowCount();
            for(var i=0; i<count; i++) {
                var idx = sensorModel.index(i,0);
                var sName = sensorModel.data(idx, 258); // NameRole

                // 1. Создаем пустую серию
                var s = chart.createSeries(ChartView.SeriesTypeLine, sName, axisX, axisY);
                s.color = getSensorColor(i);
                s.width = 2;

                // 2. Отправляем серию в C++ на заполнение
                // Это происходит МГНОВЕННО (нет цикла в JS)
                sensorModel.fillSeries(s, i, isCorrected, "A");
            }
        }
        // --- РЕЖИМ: ОДИН ДАТЧИК ---
        else {
            var idx2 = sensorModel.index(currentIndex, 0);
            var sName2 = sensorModel.data(idx2, 258);

            // Канал A
            var sA = chart.createSeries(ChartView.SeriesTypeLine, "Канал A", axisX, axisY);
            sA.color = getSensorColor(currentIndex);
            sA.width = 2;
            sensorModel.fillSeries(sA, currentIndex, isCorrected, "A");

            // Канал B
            var sB = chart.createSeries(ChartView.SeriesTypeLine, "Канал B", axisX, axisY);
            sB.color = Qt.lighter(sA.color, 1.5);
            sB.width = 2;
            sB.style = Qt.DashLine;
            sensorModel.fillSeries(sB, currentIndex, isCorrected, "B");
        }

        // Автомасштабирование осей
        // (Для идеальной скорости лучше считать Min/Max в C++ и отдавать сюда,
        // но ChartView сам умеет подстраиваться, если ему не мешать, или можно установить фиксированные)
        axisX.applyNiceNumbers();
        axisY.applyNiceNumbers();
    }

    Component.onCompleted: updateTimer.start()
    Timer { id: updateTimer; interval: 200; onTriggered: updateChart() }
}

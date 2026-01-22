import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtCharts 2.15

Rectangle {
    id: root
    anchors.fill: parent
    color: "#f5f5f5"

    property int currentIndex: -1
    property double globalMax: 0
    property double globalMin: 0

    // --- ФУНКЦИЯ ЦВЕТА ---
    // Генерирует уникальный цвет для индекса. Используем её везде.
    function getSensorColor(idx) {
        if (idx < 0) return "black";
        // Используем золотое сечение (0.618...) для хорошего распределения оттенков
        return Qt.hsla((idx * 0.618033988749895) % 1.0, 0.75, 0.5, 1.0);
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // --- ЛЕВАЯ ПАНЕЛЬ ---
        Rectangle {
            Layout.preferredWidth: 260
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#e0e0e0"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Заголовок
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
                    color: "#f8f9fa"
                    border.color: "#e0e0e0"
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "Список датчиков"
                        font.bold: true; color: "#333"
                    }
                }

                // Кнопка "Все датчики"
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 45
                    color: root.currentIndex === -1 ? "#007bff" : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "Обзор всех датчиков"
                        color: root.currentIndex === -1 ? "white" : "black"
                        font.bold: true
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { root.currentIndex = -1; updateChart() }
                    }
                }

                // Список
                ListView {
                    id: sensorList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: sensorModel
                    spacing: 1
                    delegate: Rectangle {
                        width: parent.width; height: 40
                        color: root.currentIndex === index ? "#e7f1ff" : "#ffffff"

                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 15

                            // Цветной индикатор (теперь использует общую функцию)
                            Rectangle {
                                width: 12; height: 12; radius: 6
                                color: getSensorColor(index)
                            }

                            Text {
                                text: sensorName
                                font.bold: root.currentIndex === index
                                Layout.fillWidth: true
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: { root.currentIndex = index; updateChart() }
                        }
                    }
                }
            }
        }

        // --- ЦЕНТРАЛЬНАЯ ПАНЕЛЬ ---
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10
            Layout.margins: 10

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "white"
                radius: 4
                border.color: "#cccccc"
                clip: true // Важно, чтобы tooltip не вылетал за пределы

                ChartView {
                    id: chart
                    anchors.fill: parent
                    antialiasing: true
                    theme: ChartView.ChartThemeLight
                    animationOptions: ChartView.SeriesAnimations
                    title: "Данные телеметрии"
                    titleFont.bold: true
                    titleFont.pointSize: 12
                    legend.alignment: Qt.AlignBottom

                    ValueAxis {
                        id: axisX
                        titleText: "Время (сек)"
                        labelFormat: "%.1f"
                        tickCount: 10
                    }

                    ValueAxis {
                        id: axisY
                        titleText: "Значение"
                        labelFormat: "%.0f"
                    }
                }

                // --- TOOLTIP (Всплывающая подсказка) ---
                Rectangle {
                    id: toolTip
                    width: toolTipText.implicitWidth + 20
                    height: toolTipText.implicitHeight + 10
                    color: "#cc000000" // Полупрозрачный черный
                    radius: 4
                    visible: false
                    z: 10 // Поверх графика

                    Text {
                        id: toolTipText
                        anchors.centerIn: parent
                        color: "white"
                        font.pointSize: 10
                    }
                }
            }

            // Панель статистики
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: "white"
                radius: 4
                border.color: "#cccccc"

                RowLayout {
                    anchors.fill: parent; anchors.margins: 10; spacing: 20
                    Column {
                        visible: chart.series(0) !== null
                        Text { text: "Max: " + root.globalMax.toFixed(2); color: "green"; font.bold: true }
                        Text { text: "Min: " + root.globalMin.toFixed(2); color: "red"; font.bold: true }
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "Сохранить отчет (CSV)"
                        enabled: root.currentIndex !== -1
                        onClicked: sensorModel.generateReport(root.currentIndex)
                    }
                }
            }
        }
    }

    // --- ЛОГИКА ГРАФИКА ---
    function updateChart() {
        chart.removeAllSeries();
        var minVal = 999999.0;
        var maxVal = -999999.0;
        var minTime = 999999.0;
        var maxTime = -999999.0;
        var hasData = false;

        // Вспомогательная функция добавления линии
        function addSeries(name, dataArray, color, isDash) {
            if (!dataArray || dataArray.length === 0) return;

            var series = chart.createSeries(ChartView.SeriesTypeLine, name, axisX, axisY);
            series.color = color;
            series.width = isDash ? 1 : 2;
            if (isDash) series.style = Qt.DashLine;

            // -- ЛОГИКА HOVER (НАВЕДЕНИЯ) --
            series.hovered.connect(function(point, state) {
                if (state) {
                    // Преобразуем координаты графика в пиксели
                    var p = chart.mapToPosition(point);

                    // Обновляем текст и позицию
                    toolTipText.text = "Время: " + point.x.toFixed(2) + "\nЗнач: " + point.y.toFixed(2);

                    // Позиционируем чуть выше курсора
                    toolTip.x = p.x + 15;
                    toolTip.y = p.y - 40;

                    // Проверка границ, чтобы не улетело за экран
                    if (toolTip.x + toolTip.width > chart.width) toolTip.x = p.x - toolTip.width - 15;
                    if (toolTip.y < 0) toolTip.y = p.y + 15;

                    toolTip.visible = true;
                } else {
                    toolTip.visible = false;
                }
            });

            // Заполнение данными
            for (var k = 0; k < dataArray.length; k++) {
                var t = dataArray[k].time;
                var v = dataArray[k].v1; // Если это канал B, сюда передаем v2
                series.append(t, v);

                if (v < minVal) minVal = v;
                if (v > maxVal) maxVal = v;
                if (t < minTime) minTime = t;
                if (t > maxTime) maxTime = t;
            }
            hasData = true;
        }

        if (currentIndex === -1) {
            // == ВСЕ ДАТЧИКИ ==
            chart.title = "Обзор всех датчиков (Только канал A)";
            var count = sensorModel.rowCount();
            for (var i = 0; i < count; i++) {
                var idx = sensorModel.index(i, 0);
                var sName = sensorModel.data(idx, 258);
                var sData = sensorModel.data(idx, 259);
                // Используем ту же функцию цвета
                var col = getSensorColor(i);
                addSeries(sName, sData, col, false);
            }
        } else {
            // == ОДИН ДАТЧИК ==
            var idx2 = sensorModel.index(currentIndex, 0);
            var sName2 = sensorModel.data(idx2, 258);
            var sData2 = sensorModel.data(idx2, 259);
            chart.title = "Детально: " + sName2;

            // Получаем ТОТ ЖЕ цвет, что и в общем графике
            var mainColor = getSensorColor(currentIndex);

            // Канал A (v1) - основной цвет
            addSeries("Канал A", sData2, mainColor, false);

            // Канал B (v2) - тот же цвет, но светлее, нужно вручную создать
            // Так как JS не умеет Qt.lighter при передаче в функцию (оно сразу создает объект),
            // сделаем это отдельным циклом здесь:

            var seriesB = chart.createSeries(ChartView.SeriesTypeLine, "Канал B", axisX, axisY);
            seriesB.color = Qt.lighter(mainColor, 1.5); // Светлее
            seriesB.width = 2;
            seriesB.style = Qt.DashLine;

            // Hover для канала B
            seriesB.hovered.connect(function(point, state) {
                if (state) {
                    var p = chart.mapToPosition(point);
                    toolTipText.text = "Время: " + point.x.toFixed(2) + "\nЗнач (B): " + point.y.toFixed(2);
                    toolTip.x = p.x + 15; toolTip.y = p.y - 40;
                    toolTip.visible = true;
                } else { toolTip.visible = false; }
            });

            for (var m = 0; m < sData2.length; m++) {
                var v2 = sData2[m].v2;
                seriesB.append(sData2[m].time, v2);
                if (v2 < minVal) minVal = v2;
                if (v2 > maxVal) maxVal = v2;
            }
        }

        // Обновление осей
        if (hasData) {
            var marginY = (maxVal - minVal) * 0.1;
            if (marginY === 0) marginY = 1.0;
            axisY.min = minVal - marginY;
            axisY.max = maxVal + marginY;

            axisX.min = minTime;
            axisX.max = maxTime;

            root.globalMax = maxVal;
            root.globalMin = minVal;
        } else {
            axisX.min = 0; axisX.max = 10;
            axisY.min = 0; axisY.max = 10;
        }
    }

    Component.onCompleted: timer.start()
    Timer { id: timer; interval: 100; onTriggered: updateChart() }
}

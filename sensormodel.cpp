#include "sensormodel.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QUrl>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QtMath> // Для qAbs

// Важно для работы с графиками
#include <QtMath>
#include <QtChartsDepends>
#include <QXYSeries>
#include <QtCharts/QtCharts>
#include <QtCharts/QXYSeries>
#include <QtCharts/QAbstractSeries>


SensorModel::SensorModel(QObject *parent) : QAbstractListModel(parent) {}

int SensorModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return m_sensors.size();
}

QVariant SensorModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return {};
    int row = index.row();
    if (row < 0 || row >= m_sensors.size()) return {};

    const Sensor &s = m_sensors.at(row);
    switch (role) {
    case IdRole: return s.id;
    case NameRole: return s.name;
    case DataRole: return sensorDataToVariantList(s);
    default: return {};
    }
}

QHash<int, QByteArray> SensorModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[IdRole] = "sensorId";
    roles[NameRole] = "sensorName";
    roles[DataRole] = "sensorData";
    return roles;
}

QVariant SensorModel::sensorDataToVariantList(const Sensor &s) const {
    QVariantList list;
    list.reserve(s.data.size());
    for (const DataPoint &d : s.data) {
        QVariantMap m;
        m["time"] = d.time;
        m["v1"] = d.v1;
        m["v2"] = d.v2;
        m["v1c"] = d.v1_corr;
        m["v2c"] = d.v2_corr;
        list.append(m);
    }
    return list;
}

void SensorModel::fillSeries(QAbstractSeries *series, int sensorIndex, bool useCorrected, QString channel) {
    if (!series) return;

    QXYSeries *xySeries = qobject_cast<QXYSeries *>(series);
    if (!xySeries) return;

    if (sensorIndex < 0 || sensorIndex >= m_sensors.size()) return;

    const Sensor &s = m_sensors.at(sensorIndex);
    QList<QPointF> points;
    points.reserve(s.data.size());

    for (const DataPoint &dp : s.data) {
        double val = (channel == "A") ? (useCorrected ? dp.v1_corr : dp.v1)
                                      : (useCorrected ? dp.v2_corr : dp.v2);
        points.append(QPointF(dp.time, val));
    }

    // Очистка и замена
    xySeries->replace(points);
}

void SensorModel::calculateRanges() {
    if (m_sensors.isEmpty()) return;

    // Инициализируем экстремальными значениями
    double tMin = std::numeric_limits<double>::max();
    double tMax = std::numeric_limits<double>::lowest();
    double vMin = std::numeric_limits<double>::max();
    double vMax = std::numeric_limits<double>::lowest();

    for (const Sensor &s : m_sensors) {
        for (const DataPoint &p : s.data) {
            // Время
            if (p.time < tMin) tMin = p.time;
            if (p.time > tMax) tMax = p.time;

            // Значения (проверяем все: v1, v2, и скорректированные, чтобы график не обрезался)
            // Можно проверять только v1/v2 если режим просмотра меняется редко,
            // но лучше охватить весь диапазон сразу.
            double localMin = std::min({p.v1, p.v2, p.v1_corr, p.v2_corr});
            double localMax = std::max({p.v1, p.v2, p.v1_corr, p.v2_corr});

            if (localMin < vMin) vMin = localMin;
            if (localMax > vMax) vMax = localMax;
        }
    }

    // Добавляем небольшой отступ (5%), чтобы график не прилипал к краям
    double padding = (vMax - vMin) * 0.05;
    if (padding == 0) padding = 1.0; // Защита от плоской линии

    m_minTime = tMin;
    m_maxTime = tMax;
    m_minValue = vMin - padding;
    // Если хотите, чтобы график начинался от 0 (для сенсоров это часто полезно):
    // m_minValue = (vMin < 0) ? vMin - padding : 0;

    m_maxValue = vMax + padding;

    emit dataRangeChanged(); // Сообщаем QML
}

double SensorModel::safeDivide(double target, double current) {
    return (qAbs(current) > 1e-6) ? (target / current) : 1.0;
}

void SensorModel::preCalculateCalibration() {
    if (m_sensors.isEmpty()) return;

    int sensorCount = m_sensors.size();
    QVector<double> avgHemisphere1(sensorCount, 0.0);
    QVector<double> avgHemisphere2(sensorCount, 0.0);
    double totalIntensitySum = 0.0;

    // 1. Считаем средние
    for (int i = 0; i < sensorCount; i++) {
        const Sensor &sensor = m_sensors.at(i);
        if (sensor.data.isEmpty()) continue;

        double sumH1 = 0, sumH2 = 0;
        for (const DataPoint &dp : sensor.data) {
            sumH1 += dp.v1;
            sumH2 += dp.v2;
        }

        double size = static_cast<double>(sensor.data.size());
        avgHemisphere1[i] = sumH1 / size;
        avgHemisphere2[i] = sumH2 / size;

        double intensity = avgHemisphere1[i] + avgHemisphere2[i];
        totalIntensitySum += intensity;
    }

    // 2. Считаем эталон
    m_globalReference = totalIntensitySum / sensorCount; // Сохраняем в переменную класса
    double halfRef = m_globalReference / 2.0;

    // 3. Расчет коэффициентов и применение
    for (int i = 0; i < sensorCount; ++i) {
        Sensor &s = m_sensors[i];

        double k1 = safeDivide(halfRef, avgHemisphere1[i]);
        double k2 = safeDivide(halfRef, avgHemisphere2[i]);

        // !!! СОХРАНЯЕМ КОЭФФИЦИЕНТЫ !!!
        s.kA = k1;
        s.kB = k2;

        for (DataPoint &dp : s.data) {
            dp.v1_corr = dp.v1 * k1;
            dp.v2_corr = dp.v2 * k2;
        }
    }

    qInfo() << "Calibration calculated. Global Reference:" << m_globalReference;
}

bool SensorModel::parseTxtInternal(const QString &txtFilePath) {
    QFile file(txtFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextStream in(&file);
    QString headerLine;
    bool headerFound = false;

    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.contains("Time", Qt::CaseInsensitive) && line.contains("S1_A", Qt::CaseInsensitive)) {
            headerLine = line;
            headerFound = true;
            break;
        }
    }
    if (!headerFound) return false;

    QStringList headers = headerLine.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    struct ColInfo { int sensorId; int valueType; };
    QMap<int, ColInfo> columnMapping;
    QRegularExpression re("S(\\d+)_([AB])", QRegularExpression::CaseInsensitiveOption);

    for (int i = 0; i < headers.size(); ++i) {
        QRegularExpressionMatch match = re.match(headers[i].trimmed());
        if (match.hasMatch()) {
            columnMapping[i] = {match.captured(1).toInt(), (match.captured(2).toUpper() == "A" ? 1 : 2)};
        }
    }

    QMap<int, Sensor> tempSensors;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 2) continue;

        double time = parts[0].replace(',', '.').toDouble();
        QMap<int, DataPoint> rowPoints;

        for (int i = 1; i < parts.size(); ++i) {
            if (!columnMapping.contains(i)) continue;
            ColInfo info = columnMapping[i];
            double val = parts[i].replace(',', '.').toDouble();
            if (!rowPoints.contains(info.sensorId)) rowPoints[info.sensorId] = {time, 0, 0, 0, 0};
            if (info.valueType == 1) rowPoints[info.sensorId].v1 = val;
            else rowPoints[info.sensorId].v2 = val;
        }

        for (auto it = rowPoints.begin(); it != rowPoints.end(); ++it) {
            if (!tempSensors.contains(it.key())) {
                tempSensors[it.key()].id = it.key();
                tempSensors[it.key()].name = QString("Sensor %1").arg(it.key());
            }
            tempSensors[it.key()].data.append(it.value());
        }
    }

    beginResetModel();
    m_sensors.clear();
    QList<int> keys = tempSensors.keys();
    std::sort(keys.begin(), keys.end());
    for (int key : keys) m_sensors.append(tempSensors[key]);
    preCalculateCalibration();
    calculateRanges();
    endResetModel();
    return true;
}

void SensorModel::importFromTxt(const QString &fileUrl) {
    QString path = QUrl(fileUrl).toLocalFile();
    if (path.isEmpty()) path = fileUrl;
    parseTxtInternal(path);
}

// ИСПРАВЛЕННЫЙ ЭКСПОРТ
void SensorModel::exportToCsv(const QString &fileUrl) {
    QString path = QUrl(fileUrl).toLocalFile();
    if (path.isEmpty()) path = fileUrl;
    if (!path.endsWith(".csv", Qt::CaseInsensitive)) path += ".csv";

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << "SensorID,SensorName,Time,Raw_A,Raw_B,Corrected_A,Corrected_B\n";
        for (const Sensor &s : m_sensors) {
            for (const DataPoint &d : s.data) {
                out << s.id << "," << s.name << ","
                    << QString::number(d.time, 'f', 3) << ","
                    << QString::number(d.v1, 'f', 2) << ","
                    << QString::number(d.v2, 'f', 2) << ","
                    << QString::number(d.v1_corr, 'f', 4) << ","
                    << QString::number(d.v2_corr, 'f', 4) << "\n";
            }
        }
        f.close();
    }
}

// ИСПРАВЛЕННЫЙ ОТЧЕТ
void SensorModel::generateReport(int index) {
    if (index < 0 || index >= m_sensors.size()) return;
    const Sensor &s = m_sensors.at(index);

    QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString filename = QString("%1/sensor_%2_report.csv").arg(docs).arg(s.id);

    QFile f(filename);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << "Time,Raw_A,Raw_B\n";
        for (const DataPoint &d : s.data) {
            out << d.time << "," << d.v1 << "," << d.v2 << "\n";
        }
        f.close();
    }
}

// QVariantMap SensorModel::getSensorStats(int index) {
//     QVariantMap map;

//     // Если выбраны "Все сенсоры" (index == -1)
//     if (index < 0 || index >= m_sensors.size()) {
//         map["type"] = "all";
//         map["reference"] = m_globalReference; // Глобальный эталон яркости

//         // Считаем средний разброс коэффициентов (насколько сенсоры "кривые" в среднем)
//         double totalDev = 0;
//         int count = 0;
//         for(const Sensor &s : m_sensors) {
//             // Отклонение от 1.0 (идеала)
//             totalDev += qAbs(1.0 - s.kA) + qAbs(1.0 - s.kB);
//             count += 2;
//         }
//         double avgDev = (count > 0) ? (totalDev / count) : 0.0;
//         map["avgCorrection"] = avgDev;
//         return map;
//     }

//     // Если выбран конкретный сенсор
//     const Sensor &s = m_sensors.at(index);
//     map["type"] = "single";
//     map["name"] = s.name;
//     map["kA"] = s.kA;
//     map["kB"] = s.kB;

//     // Дополнительно можно вернуть среднее сырое значение
//     double rawAvgA = 0, rawAvgB = 0;
//     if (!s.data.isEmpty()) {
//         for(const auto& p : s.data) { rawAvgA += p.v1; rawAvgB += p.v2; }
//         rawAvgA /= s.data.size();
//         rawAvgB /= s.data.size();
//     }
//     map["rawA"] = rawAvgA;
//     map["rawB"] = rawAvgB;

//     return map;
// }

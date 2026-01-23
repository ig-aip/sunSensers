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

double SensorModel::safeDivide(double target, double current) {
    return (qAbs(current) > 1e-6) ? (target / current) : 1.0;
}

void SensorModel::preCalculateCalibration() {
    if (m_sensors.isEmpty()) return;
    int sensorCount = m_sensors.size();
    QVector<double> avgH1(sensorCount, 0.0);
    QVector<double> avgH2(sensorCount, 0.0);
    double totalIntensitySum = 0.0;

    for (int i = 0; i < sensorCount; i++) {
        const Sensor &sensor = m_sensors.at(i);
        if (sensor.data.isEmpty()) continue;
        double sumH1 = 0, sumH2 = 0;
        for (const DataPoint &dp : sensor.data) {
            sumH1 += dp.v1;
            sumH2 += dp.v2;
        }
        avgH1[i] = sumH1 / sensor.data.size();
        avgH2[i] = sumH2 / sensor.data.size();
        totalIntensitySum += (avgH1[i] + avgH2[i]);
    }

    double referenceValue = totalIntensitySum / sensorCount;
    double halfRef = referenceValue / 2.0;

    for (int i = 0; i < sensorCount; ++i) {
        Sensor &s = m_sensors[i];
        double k1 = safeDivide(halfRef, avgH1[i]);
        double k2 = safeDivide(halfRef, avgH2[i]);
        for (DataPoint &dp : s.data) {
            dp.v1_corr = dp.v1 * k1;
            dp.v2_corr = dp.v2 * k2;
        }
    }
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

#include "sensormodel.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QUrl>
#include <QStandardPaths>
#include <QDir>
#include <QtMath>
#include <QFileInfo>

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

// Вспомогательный метод для QML (если нужно передавать сырые данные)
QVariant SensorModel::sensorDataToVariantList(const Sensor &s) const {
    QVariantList list;
    // Оптимизация: не передаем огромные списки в QML без нужды,
    // графики рисуются через C++ (fillSeries)
    return list;
}

// ---------------------------------------------------------
// ШАГ 1: Конвертация TXT -> JSON
// ---------------------------------------------------------
bool SensorModel::convertTxtToJson(const QString &txtFilePath, const QString &jsonFilePath) {
    QFile file(txtFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open TXT file:" << txtFilePath;
        return false;
    }

    QTextStream in(&file);
    QString headerLine;
    bool headerFound = false;

    // Поиск заголовка
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.contains("Time", Qt::CaseInsensitive) && line.contains("_A", Qt::CaseInsensitive)) {
            headerLine = line;
            headerFound = true;
            break;
        }
    }

    if (!headerFound) return false;

    // Парсинг заголовков столбцов
    QStringList headers = headerLine.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    struct ColInfo { int sensorId; QString type; }; // type: "A" или "B"
    QMap<int, ColInfo> colMap;

    QRegularExpression re("S(\\d+)_([AB])", QRegularExpression::CaseInsensitiveOption);

    for (int i = 0; i < headers.size(); ++i) {
        QRegularExpressionMatch match = re.match(headers[i].trimmed());
        if (match.hasMatch()) {
            colMap[i] = { match.captured(1).toInt(), match.captured(2).toUpper() };
        }
    }

    // Временное хранилище данных: Map<SensorID, List of Objects>
    // Используем QJsonObject для каждой точки, но группируем по ID сенсора
    QMap<int, QJsonArray> sensorDataArrays;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // Заменяем запятую на точку для корректного парсинга чисел
        line.replace(',', '.');

        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 2) continue;

        double time = parts[0].toDouble();

        // Проходим по столбцам и раскидываем значения по сенсорам
        // Сложность: в одной строке данные для всех сенсоров сразу.
        // Нам нужно преобразовать это в структуру: Sensor -> Array({time, v1, v2})

        // Сначала собираем временную строку для каждого сенсора
        QMap<int, QJsonObject> rowPoints;

        for (int i = 1; i < parts.size(); ++i) {
            if (!colMap.contains(i)) continue;

            ColInfo info = colMap[i];
            double val = parts[i].toDouble();

            if (!rowPoints.contains(info.sensorId)) {
                QJsonObject p;
                p["t"] = time;
                p["v1"] = 0.0;
                p["v2"] = 0.0;
                rowPoints[info.sensorId] = p;
            }

            QJsonObject p = rowPoints[info.sensorId];
            if (info.type == "A") p["v1"] = val;
            else p["v2"] = val;
            rowPoints[info.sensorId] = p;
        }

        // Добавляем точки в массивы сенсоров
        auto it = rowPoints.constBegin();
        while (it != rowPoints.constEnd()) {
            sensorDataArrays[it.key()].append(it.value());
            ++it;
        }
    }
    file.close();

    // Формируем итоговый JSON объект
    QJsonArray sensorsArray;
    auto it = sensorDataArrays.constBegin();
    while (it != sensorDataArrays.constEnd()) {
        QJsonObject sensorObj;
        sensorObj["id"] = it.key();
        sensorObj["name"] = QString("Sensor %1").arg(it.key());
        sensorObj["data"] = it.value(); // Массив точек {t, v1, v2}
        sensorsArray.append(sensorObj);
        ++it;
    }

    QJsonObject root;
    root["sensors"] = sensorsArray;

    // Запись JSON на диск
    QFile jsonFile(jsonFilePath);
    if (!jsonFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Could not write JSON file:" << jsonFilePath;
        return false;
    }
    QJsonDocument doc(root);
    jsonFile.write(doc.toJson(QJsonDocument::Compact)); // Compact для экономии места
    jsonFile.close();

    qInfo() << "Converted TXT to JSON:" << jsonFilePath;
    return true;
}

// ---------------------------------------------------------
// ШАГ 2: Парсинг JSON -> C++ Model
// ---------------------------------------------------------
bool SensorModel::parseJsonInternal(const QString &jsonFilePath) {
    QFile file(jsonFilePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull()) return false;

    QJsonObject root = doc.object();
    QJsonArray sensorsArray = root["sensors"].toArray();

    beginResetModel();
    m_sensors.clear();

    for (const QJsonValue &sVal : sensorsArray) {
        QJsonObject sObj = sVal.toObject();
        Sensor sensor;
        sensor.id = sObj["id"].toInt();
        sensor.name = sObj["name"].toString();

        QJsonArray dataArr = sObj["data"].toArray();
        sensor.data.reserve(dataArr.size());

        for (const QJsonValue &dVal : dataArr) {
            QJsonObject dObj = dVal.toObject();
            DataPoint dp;
            dp.time = dObj["t"].toDouble();
            dp.v1 = dObj["v1"].toDouble();
            dp.v2 = dObj["v2"].toDouble();
            // v1_corr и v2_corr будут рассчитаны позже в preCalculateCalibration
            dp.v1_corr = dp.v1;
            dp.v2_corr = dp.v2;
            sensor.data.append(dp);
        }
        m_sensors.append(sensor);
    }

    // Сортировка по ID для порядка в списке
    std::sort(m_sensors.begin(), m_sensors.end(), [](const Sensor& a, const Sensor& b){
        return a.id < b.id;
    });

    preCalculateCalibration(); // Математика
    calculateRanges();         // Границы для графика
    endResetModel();

    return true;
}

// ---------------------------------------------------------
// Основной метод, вызываемый из QML или main.cpp
// ---------------------------------------------------------
void SensorModel::importFromTxt(const QString &fileUrl) {
    QString txtPath = QUrl(fileUrl).toLocalFile();
    if (txtPath.isEmpty()) txtPath = fileUrl;

    // Создаем путь для промежуточного JSON файла
    QFileInfo fi(txtPath);
    QString jsonPath = fi.absolutePath() + "/" + fi.baseName() + ".json";

    qInfo() << "Starting import pipeline...";

    // 1. TXT -> JSON
    if (convertTxtToJson(txtPath, jsonPath)) {
        // 2. JSON -> App
        if (parseJsonInternal(jsonPath)) {
            qInfo() << "Import successful via JSON.";
        } else {
            qWarning() << "Failed to parse generated JSON.";
        }
    } else {
        qWarning() << "Failed to convert TXT to JSON.";
    }
}

// Метод для автоматической загрузки при старте (использует тот же конвейер)
bool SensorModel::loadResultsFile(const QString &filePath) {
    if (!QFile::exists(filePath)) return false;
    importFromTxt(filePath);
    return !m_sensors.isEmpty();
}

// ---------------------------------------------------------
// Остальная логика (математика, графики) без изменений
// ---------------------------------------------------------

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
    xySeries->replace(points);
}

void SensorModel::calculateRanges() {
    if (m_sensors.isEmpty()) return;
    double tMin = std::numeric_limits<double>::max();
    double tMax = std::numeric_limits<double>::lowest();
    double vMin = std::numeric_limits<double>::max();
    double vMax = std::numeric_limits<double>::lowest();

    for (const Sensor &s : m_sensors) {
        for (const DataPoint &p : s.data) {
            if (p.time < tMin) tMin = p.time;
            if (p.time > tMax) tMax = p.time;
            double localMin = std::min({p.v1, p.v2});
            double localMax = std::max({p.v1, p.v2});
            if (localMin < vMin) vMin = localMin;
            if (localMax > vMax) vMax = localMax;
        }
    }
    double padding = (vMax - vMin) * 0.05;
    if (padding == 0) padding = 1.0;
    m_minTime = tMin;
    m_maxTime = tMax;
    m_minValue = vMin - padding;
    m_maxValue = vMax + padding;
    emit dataRangeChanged();
}

double SensorModel::safeDivide(double target, double current) {
    return (qAbs(current) > 1e-6) ? (target / current) : 1.0;
}

void SensorModel::preCalculateCalibration() {
    if (m_sensors.isEmpty()) return;
    int sensorCount = m_sensors.size();
    double totalIntensitySum = 0.0;
    QVector<double> avgH1(sensorCount, 0), avgH2(sensorCount, 0);

    for (int i = 0; i < sensorCount; i++) {
        const Sensor &sensor = m_sensors.at(i);
        if (sensor.data.isEmpty()) continue;
        double sum1 = 0, sum2 = 0;
        for (const DataPoint &dp : sensor.data) { sum1 += dp.v1; sum2 += dp.v2; }
        avgH1[i] = sum1 / sensor.data.size();
        avgH2[i] = sum2 / sensor.data.size();
        totalIntensitySum += (avgH1[i] + avgH2[i]);
    }

    m_globalReference = totalIntensitySum / sensorCount;
    double halfRef = m_globalReference / 2.0;

    for (int i = 0; i < sensorCount; ++i) {
        Sensor &s = m_sensors[i];
        s.kA = safeDivide(halfRef, avgH1[i]);
        s.kB = safeDivide(halfRef, avgH2[i]);
        for (DataPoint &dp : s.data) {
            dp.v1_corr = dp.v1 * s.kA;
            dp.v2_corr = dp.v2 * s.kB;
        }
    }
}

void SensorModel::exportToCsv(const QString &fileUrl) {
    QString path = QUrl(fileUrl).toLocalFile();
    if (path.isEmpty()) path = fileUrl;
    if (!path.endsWith(".csv", Qt::CaseInsensitive)) path += ".csv";
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << "SensorID,SensorName,Time,Raw_A,Raw_B,Corr_A,Corr_B\n";
        for (const Sensor &s : m_sensors) {
            for (const DataPoint &d : s.data) {
                out << s.id << "," << s.name << "," << QString::number(d.time, 'f', 3) << ","
                    << d.v1 << "," << d.v2 << "," << d.v1_corr << "," << d.v2_corr << "\n";
            }
        }
    }
}

void SensorModel::generateReport(int index) {
    // Реализация отчета (оставляем как было или заглушку)
}

QVariantMap SensorModel::getSensorStats(int index) {
    QVariantMap map;
    if (index < 0 || index >= m_sensors.size()) {
        map["type"] = "all";
        map["reference"] = m_globalReference;
        double totalDev = 0;
        int count = 0;
        for(const Sensor &s : m_sensors) {
            totalDev += qAbs(1.0 - s.kA) + qAbs(1.0 - s.kB);
            count += 2;
        }
        map["avgCorrection"] = (count > 0) ? (totalDev / count) : 0.0;
        return map;
    }
    const Sensor &s = m_sensors.at(index);
    map["type"] = "single";
    map["name"] = s.name;
    map["kA"] = s.kA;
    map["kB"] = s.kB;
    map["pA"] = qAbs(1 - s.kA) * 100;
    map["pB"] = qAbs(1 - s.kB) * 100;
    double ra = 0, rb = 0;
    if (!s.data.isEmpty()) {
        for(const auto& p : s.data) { ra += p.v1; rb += p.v2; }
        ra /= s.data.size(); rb /= s.data.size();
    }
    map["rawA"] = ra; map["rawB"] = rb;
    return map;
}

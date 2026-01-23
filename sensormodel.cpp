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
#include <QDateTime>
#include <QCoreApplication>

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
    return QVariantList(); // Оптимизация: данные не гоняем через модель в QML
}

// ---------------------------------------------------------
// ЭКСПОРТ JSON (С коэффициентами и статистикой)
// ---------------------------------------------------------
void SensorModel::exportToJson(const QString &fileUrl) {
    QString path = QUrl(fileUrl).toLocalFile();
    if (path.isEmpty()) path = fileUrl;
    if (!path.endsWith(".json", Qt::CaseInsensitive)) path += ".json";

    // 1. Статистика
    double totalDev = 0;
    int count = 0;
    for(const Sensor &s : m_sensors) {
        totalDev += qAbs(1.0 - s.kA) + qAbs(1.0 - s.kB);
        count += 2;
    }
    double avgDevPercent = (count > 0) ? (totalDev / count) * 100.0 : 0.0;

    QJsonObject statsObj;
    statsObj["global_reference_value"] = m_globalReference;
    statsObj["average_system_deviation_percent"] = QString::number(avgDevPercent, 'f', 2).toDouble();
    statsObj["total_sensors_count"] = m_sensors.size();

    // 2. Сенсоры
    QJsonArray sensorsArr;
    for (const Sensor &s : m_sensors) {
        QJsonObject sObj;
        sObj["id"] = s.id;
        sObj["name"] = s.name;

        QJsonObject calibObj;
        calibObj["coeff_A"] = s.kA;
        calibObj["coeff_B"] = s.kB;
        calibObj["error_A_percent"] = QString::number((s.kA - 1.0) * 100.0, 'f', 2).toDouble();
        calibObj["error_B_percent"] = QString::number((s.kB - 1.0) * 100.0, 'f', 2).toDouble();
        sObj["calibration"] = calibObj;

        QJsonArray dataArr;
        for (const DataPoint &dp : s.data) {
            QJsonObject p;
            p["t"] = QString::number(dp.time, 'f', 3).toDouble();
            p["raw_A"] = dp.v1;
            p["raw_B"] = dp.v2;
            p["corr_A"] = QString::number(dp.v1_corr, 'f', 2).toDouble();
            p["corr_B"] = QString::number(dp.v2_corr, 'f', 2).toDouble();
            dataArr.append(p);
        }
        sObj["data"] = dataArr;
        sensorsArr.append(sObj);
    }

    QJsonObject root;
    root["meta_info"] = QJsonObject{
        {"exported_at", QDateTime::currentDateTime().toString(Qt::ISODate)},
        {"app_name", "SolarSensors Analytics"}
    };
    root["statistics"] = statsObj;
    root["sensors"] = sensorsArr;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(root);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        qInfo() << "Exported JSON to:" << path;
    } else {
        qWarning() << "Failed to save JSON:" << path;
    }
}

// ---------------------------------------------------------
// Конвертация TXT -> JSON
// ---------------------------------------------------------
bool SensorModel::convertTxtToJson(const QString &txtFilePath, const QString &jsonFilePath) {
    QFile file(txtFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextStream in(&file);
    QString headerLine;
    bool headerFound = false;

    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.contains("Time", Qt::CaseInsensitive) && line.contains("_A", Qt::CaseInsensitive)) {
            headerLine = line;
            headerFound = true;
            break;
        }
    }
    if (!headerFound) return false;

    QStringList headers = headerLine.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    struct ColInfo { int sensorId; QString type; };
    QMap<int, ColInfo> colMap;
    QRegularExpression re("S(\\d+)_([AB])", QRegularExpression::CaseInsensitiveOption);

    for (int i = 0; i < headers.size(); ++i) {
        QRegularExpressionMatch match = re.match(headers[i].trimmed());
        if (match.hasMatch()) {
            colMap[i] = { match.captured(1).toInt(), match.captured(2).toUpper() };
        }
    }

    QMap<int, QJsonArray> sensorDataArrays;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        line.replace(',', '.');
        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 2) continue;

        double time = parts[0].toDouble();
        QMap<int, QJsonObject> rowPoints;

        for (int i = 1; i < parts.size(); ++i) {
            if (!colMap.contains(i)) continue;
            ColInfo info = colMap[i];
            double val = parts[i].toDouble();
            if (!rowPoints.contains(info.sensorId)) {
                QJsonObject p; p["t"] = time; p["v1"] = 0.0; p["v2"] = 0.0;
                rowPoints[info.sensorId] = p;
            }
            if (info.type == "A") rowPoints[info.sensorId]["v1"] = val;
            else rowPoints[info.sensorId]["v2"] = val;
        }
        auto it = rowPoints.constBegin();
        while (it != rowPoints.constEnd()) {
            sensorDataArrays[it.key()].append(it.value());
            ++it;
        }
    }
    file.close();

    QJsonArray sensorsArray;
    auto it = sensorDataArrays.constBegin();
    while (it != sensorDataArrays.constEnd()) {
        QJsonObject sensorObj;
        sensorObj["id"] = it.key();
        sensorObj["name"] = QString("Sensor %1").arg(it.key());
        sensorObj["data"] = it.value();
        sensorsArray.append(sensorObj);
        ++it;
    }

    QJsonObject root;
    root["sensors"] = sensorsArray;

    QFile jsonFile(jsonFilePath);
    if (!jsonFile.open(QIODevice::WriteOnly)) return false;
    QJsonDocument doc(root);
    jsonFile.write(doc.toJson(QJsonDocument::Indented));
    jsonFile.close();
    return true;
}

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
            dp.v1_corr = dp.v1;
            dp.v2_corr = dp.v2;
            sensor.data.append(dp);
        }
        m_sensors.append(sensor);
    }

    std::sort(m_sensors.begin(), m_sensors.end(), [](const Sensor& a, const Sensor& b){ return a.id < b.id; });

    preCalculateCalibration();
    calculateRanges();
    endResetModel();
    return true;
}

void SensorModel::importFromTxt(const QString &fileUrl) {
    QString txtPath = QUrl(fileUrl).toLocalFile();
    if (txtPath.isEmpty()) txtPath = fileUrl;

    QFileInfo fi(txtPath);

    // 1. Создаем временный файл для первичного парсинга (в папке с исходником)

    QString tempJsonPath = fi.absolutePath() + "/" + fi.baseName() + "_temp_parsing.json";

    qInfo() << "Step 1: Converting TXT to Temp JSON...";

    if (convertTxtToJson(txtPath, tempJsonPath)) {
        // 2. Загружаем данные в модель
        if (parseJsonInternal(tempJsonPath)) {
            qInfo() << "Step 2: Data loaded & Math calculated.";


            // Получаем путь к папке
            QString exeDir = QCoreApplication::applicationDirPath();

            // имя файла
            QString finalJsonName = fi.baseName() + ".json";
            QString finalJsonPath = QDir(exeDir).filePath(finalJsonName);

            qInfo() << "Step 3: Auto-generating Final JSON at:" << finalJsonPath;


            exportToJson(finalJsonPath);

            QFile::remove(tempJsonPath);
        } else {
            qWarning() << "Failed to parse generated temp JSON.";
        }
    } else {
        qWarning() << "Failed to convert TXT to JSON.";
    }
}

bool SensorModel::loadResultsFile(const QString &filePath) {
    if (!QFile::exists(filePath)) return false;
    importFromTxt(filePath);
    return !m_sensors.isEmpty();
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
        double val = (channel == "A") ? (useCorrected ? dp.v1_corr : dp.v1) : (useCorrected ? dp.v2_corr : dp.v2);
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
    // 1. Превращаем QML URL в локальный путь
    QString path = QUrl(fileUrl).toLocalFile();
    if (path.isEmpty()) path = fileUrl;
    if (!path.endsWith(".csv", Qt::CaseInsensitive)) path += ".csv";

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Не удалось открыть файл для записи:" << path;
        return;
    }

    QTextStream out(&file);
    // Устанавливаем кодировку UTF-8 и BOM для корректного открытия в Excel
    out.setGenerateByteOrderMark(true);

    if (m_sensors.isEmpty()) {
        file.close();
        return;
    }

    // 2. Создаем заголовок таблицы
    // Чтобы соответствовать JSON, выводим и сырые, и скорректированные данные
    QStringList header;
    header << "Time (s)";

    for (const Sensor &s : m_sensors) {
        // Добавляем информацию о коэффициентах в заголовок (опционально, для удобства)
        QString prefix = s.name; // Например "Sensor 1"

        header << (prefix + " Raw A")
               << (prefix + " Raw B")
               << (prefix + " Corr A (x" + QString::number(s.kA, 'f', 3) + ")")
               << (prefix + " Corr B (x" + QString::number(s.kB, 'f', 3) + ")");
    }

    // Используем ';' как разделитель (стандарт для Excel в РФ/Европе)
    out << header.join(";") << "\n";

    // 3. Собираем данные
    // Находим максимальное количество строк (на случай рассинхрона датчиков)
    int maxRows = 0;
    for(const auto& s : m_sensors) {
        if(s.data.size() > maxRows) maxRows = s.data.size();
    }

    for (int i = 0; i < maxRows; ++i) {
        QStringList row;

        // Берем время из первого датчика (или 0, если данных нет)
        double timeVal = 0.0;
        if (!m_sensors.isEmpty() && i < m_sensors[0].data.size()) {
            timeVal = m_sensors[0].data[i].time;
        }

        // Записываем время (заменяем точку на запятую для Excel, если нужно, но пока оставим стандарт)
        row << QString::number(timeVal, 'f', 3);

        for (const Sensor &s : m_sensors) {
            if (i < s.data.size()) {
                const DataPoint &p = s.data[i];

                // Сырые данные (как в JSON "raw_A", "raw_B")
                row << QString::number(p.v1, 'f', 0); // Обычно сырые - целые числа
                row << QString::number(p.v2, 'f', 0);

                // Скорректированные данные (как в JSON "corr_A", "corr_B")
                row << QString::number(p.v1_corr, 'f', 2);
                row << QString::number(p.v2_corr, 'f', 2);
            } else {
                // Если данные у этого датчика кончились раньше
                row << "" << "" << "" << "";
            }
        }
        out << row.join(";") << "\n";
    }

    file.close();
    qInfo() << "Full CSV Export completed:" << path;
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
    // Считаем погрешности в % для QML
    map["pA"] = qAbs(s.kA - 1.0) * 100.0;
    map["pB"] = qAbs(s.kB - 1.0) * 100.0;

    double avgRawA = 0, avgRawB = 0;
    if (!s.data.isEmpty()) {
        double sumRawA = 0, sumRawB = 0;
        for (const DataPoint& data : s.data) {
            sumRawA += data.v1;
            sumRawB += data.v2;
        }
        avgRawA = sumRawA / s.data.size();
        avgRawB = sumRawB / s.data.size();
    }
    map["avgRawA"] = avgRawA; map["avgRawB"] = avgRawB;
    return map;
}

#include "sensormodel.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

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
        list.append(m);
    }
    return list;
}

bool SensorModel::convertTxtToJson(const QString &txtFilePath, const QString &jsonFilePath) {
    QFile file(txtFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open input TXT file:" << txtFilePath;
        return false;
    }

    QTextStream in(&file);
    QString headerLine;
    bool headerFound = false;

    // 1. Ищем строку с заголовками
    while (!in.atEnd()) {
        QString line = in.readLine();
        // Проверяем наличие ключевых слов (S1_A и Time)
        if (line.contains("Time") && line.contains("S1_A")) {
            headerLine = line;
            headerFound = true;
            break;
        }
    }

    if (!headerFound) {
        qWarning() << "Headers not found in TXT file";
        return false;
    }

    // 2. Парсим заголовки
    // ИЗМЕНЕНИЕ: Делим по любым пробельным символам (табуляция или пробел)
    QStringList headers = headerLine.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    struct ColInfo { int sensorId; int valueType; };
    QMap<int, ColInfo> columnMapping;
    QRegularExpression re("S(\\d+)_([AB])");

    for (int i = 0; i < headers.size(); ++i) {
        QString h = headers[i].trimmed();
        QRegularExpressionMatch match = re.match(h);
        if (match.hasMatch()) {
            int sId = match.captured(1).toInt();
            QString typeStr = match.captured(2);
            int vType = (typeStr == "A" ? 1 : 2);
            columnMapping[i] = {sId, vType};
        }
    }

    QMap<int, Sensor> tempSensors;

    // 3. Читаем данные
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // ИЗМЕНЕНИЕ: Делим строку по пробелам/табам
        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        // Проверка: количество колонок должно примерно совпадать (минимум Time + 1)
        if (parts.size() < 2) continue;

        // ИЗМЕНЕНИЕ: Обработка времени с запятой (0,00 -> 0.00)
        QString timeStr = parts[0];
        timeStr.replace(',', '.'); // Заменяем запятую на точку для правильного парсинга

        bool okTime = false;
        double time = timeStr.toDouble(&okTime);
        if (!okTime) continue;

        QMap<int, DataPoint> rowPoints;

        for (int i = 1; i < parts.size(); ++i) {
            if (!columnMapping.contains(i)) continue;

            ColInfo info = columnMapping[i];

            // Данные значений тоже могут быть с запятой (на всякий случай заменяем)
            QString valStr = parts[i];
            valStr.replace(',', '.');
            double val = valStr.toDouble();

            if (!rowPoints.contains(info.sensorId)) {
                rowPoints[info.sensorId] = {time, 0.0, 0.0};
            }

            if (info.valueType == 1) rowPoints[info.sensorId].v1 = val;
            else rowPoints[info.sensorId].v2 = val;
        }

        QMapIterator<int, DataPoint> it(rowPoints);
        while (it.hasNext()) {
            it.next();
            int sId = it.key();
            if (!tempSensors.contains(sId)) {
                Sensor s;
                s.id = sId;
                s.name = QString("Sensor %1").arg(sId);
                tempSensors[sId] = s;
            }
            tempSensors[sId].data.append(it.value());
        }
    }
    file.close();

    // 4. Формируем JSON
    QJsonArray sensorsArray;
    QList<int> keys = tempSensors.keys();
    std::sort(keys.begin(), keys.end());

    for (int key : keys) {
        const Sensor &s = tempSensors[key];
        QJsonObject sObj;
        sObj["id"] = s.id;
        sObj["name"] = s.name;

        QJsonArray dataArray;
        for (const DataPoint &dp : s.data) {
            QJsonObject dpObj;
            dpObj["time"] = dp.time;
            dpObj["v1"] = dp.v1;
            dpObj["v2"] = dp.v2;
            dataArray.append(dpObj);
        }
        sObj["data"] = dataArray;
        sensorsArray.append(sObj);
    }

    QJsonObject root;
    root["sensors"] = sensorsArray;

    QFile jsonFile(jsonFilePath);
    if (!jsonFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open output JSON file";
        return false;
    }
    jsonFile.write(QJsonDocument(root).toJson());
    jsonFile.close();

    qInfo() << "Successfully converted TXT (Tab Separated) to JSON:" << jsonFilePath;
    return true;
}

void SensorModel::loadFromJsonFile(const QString &filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open JSON file:" << filePath;
        return;
    }
    const QByteArray raw = f.readAll();
    f.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    QJsonObject root = doc.object();
    QJsonArray arr = root["sensors"].toArray();

    beginResetModel();
    m_sensors.clear();
    for (const QJsonValue &val : arr) {
        QJsonObject so = val.toObject();
        Sensor s;
        s.id = so["id"].toInt();
        s.name = so["name"].toString();

        QJsonArray da = so["data"].toArray();
        s.data.reserve(da.size());
        for (const QJsonValue &dv : da) {
            QJsonObject dobj = dv.toObject();
            DataPoint dp;
            dp.time = dobj["time"].toDouble();
            dp.v1 = dobj["v1"].toDouble();
            dp.v2 = dobj["v2"].toDouble();
            s.data.append(dp);
        }
        m_sensors.append(s);
    }
    endResetModel();
}

void SensorModel::generateReport(int index) {
    // Без изменений, код из предыдущего ответа
    if (index < 0 || index >= m_sensors.size()) return;
    const Sensor &s = m_sensors.at(index);
    QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString filename = QString("%1/sensor_%2.csv").arg(docs).arg(s.id);
    QFile f(filename);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << "Time,Value_A,Value_B\n";
        for (const DataPoint &d : s.data) out << d.time << "," << d.v1 << "," << d.v2 << "\n";
        f.close();
    }
}

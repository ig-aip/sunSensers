#ifndef SENSORMODEL_H
#define SENSORMODEL_H
#include <QString>
#include <QAbstractListModel>

// struct Sensor{
//     int id;
//     std::pair<int, int> values;
// };

struct DataPoint {
    double time;
    double v1;
    double v2;
};

struct Sensor {
    int id;
    QString name;
    QVector<DataPoint> data;
};

class SensorModel : public QAbstractListModel{
Q_OBJECT
public:

// Роли для QML. Важно: Qt::UserRole обычно равен 256.
// NameRole = 258, DataRole = 259
    enum Roles {
        IdRole = Qt::UserRole + 1, // = 257
        NameRole,                  // = 258
        DataRole                   // = 259
    };

explicit SensorModel(QObject *parent = nullptr);

int rowCount(const QModelIndex &parent = QModelIndex()) const override;
QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
QHash<int, QByteArray> roleNames() const override;

// Метод парсит raw .txt и сохраняет структурированный .json
Q_INVOKABLE bool convertTxtToJson(const QString &txtFilePath, const QString &jsonFilePath);

// Метод загружает готовый .json в модель
Q_INVOKABLE void loadFromJsonFile(const QString &filePath);

// Генерация CSV отчета
Q_INVOKABLE void generateReport(int index);

private:
QVector<Sensor> m_sensors;
QVariant sensorDataToVariantList(const Sensor &s) const;
};

#endif // SENSORMODEL_H

#ifndef SENSORMODEL_H
#define SENSORMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <QString>
#include <QVariant>
#include <QPointF>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

#include <QtCharts/QAbstractSeries>
#include <QtCharts/QXYSeries>

struct DataPoint {
    double time;
    double v1;      // Канал A
    double v2;      // Канал B
    double v1_corr; // Скорректированный A
    double v2_corr; // Скорректированный B
};

struct Sensor {
    int id;
    QString name;
    QVector<DataPoint> data;
    double kA = 1.0;
    double kB = 1.0;
};

class SensorModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(double minTime READ minTime NOTIFY dataRangeChanged)
    Q_PROPERTY(double maxTime READ maxTime NOTIFY dataRangeChanged)
    Q_PROPERTY(double minValue READ minValue NOTIFY dataRangeChanged)
    Q_PROPERTY(double maxValue READ maxValue NOTIFY dataRangeChanged)
    Q_PROPERTY(double globalReference READ globalReference NOTIFY dataRangeChanged)

public:
    enum Roles { IdRole = Qt::UserRole + 1, NameRole, DataRole };

    explicit SensorModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    double globalReference() const { return m_globalReference; }

    // Основной метод импорта: вызывает TXT->JSON, затем JSON->Model
    Q_INVOKABLE void importFromTxt(const QString &fileUrl);

    Q_INVOKABLE void exportToCsv(const QString &fileUrl);
    Q_INVOKABLE void generateReport(int index);
    Q_INVOKABLE void fillSeries(QAbstractSeries *series, int sensorIndex, bool useCorrected, QString channel);
    Q_INVOKABLE QVariantMap getSensorStats(int index);

    double minTime() const { return m_minTime; }
    double maxTime() const { return m_maxTime; }
    double minValue() const { return m_minValue; }
    double maxValue() const { return m_maxValue; }

    // Логика автоматической загрузки при старте
    bool loadResultsFile(const QString &filePath);

signals:
    void dataRangeChanged();

private:
    void calculateRanges();
    void preCalculateCalibration();
    static double safeDivide(double target, double current);
    QVariant sensorDataToVariantList(const Sensor &s) const;

    // --- НОВЫЕ МЕТОДЫ ---
    // 1. Конвертирует TXT в JSON файл
    bool convertTxtToJson(const QString &txtFilePath, const QString &jsonFilePath);
    // 2. Парсит JSON файл в структуру Sensor
    bool parseJsonInternal(const QString &jsonFilePath);

    QVector<Sensor> m_sensors;

    double m_minTime = 0.0;
    double m_maxTime = 10.0;
    double m_minValue = 0.0;
    double m_maxValue = 100.0;
    double m_globalReference = 0.0;
};

#endif // SENSORMODEL_H

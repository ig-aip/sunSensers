#ifndef SENSORMODEL_H
#define SENSORMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <QString>
#include <QVariant>
#include <QPointF>

// Обязательно подключаем заголовки
#include <QtCharts/QAbstractSeries>
#include <QtCharts/QXYSeries>

// Активируем пространство имен QtCharts

    struct DataPoint {
    double time;
    double v1;
    double v2;
    double v1_corr;
    double v2_corr;
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

    //QVariantMap getSensorStats(int index);

    Q_INVOKABLE void importFromTxt(const QString &fileUrl);
    Q_INVOKABLE void exportToCsv(const QString &fileUrl);
    Q_INVOKABLE void generateReport(int index);

    // Используем QAbstractSeries
    Q_INVOKABLE void fillSeries(QAbstractSeries *series, int sensorIndex, bool useCorrected, QString channel);

    Q_INVOKABLE QVariantMap getSensorStats(int index);

    double minTime() const { return m_minTime; }
    double maxTime() const { return m_maxTime; }
    double minValue() const { return m_minValue; }
    double maxValue() const { return m_maxValue; }



    bool parseTxtInternal(const QString &txtFilePath);

signals:
    void dataRangeChanged();

private:
    void calculateRanges();
    QVector<Sensor> m_sensors;
    QVariant sensorDataToVariantList(const Sensor &s) const;
    void preCalculateCalibration();
    static double safeDivide(double target, double current);

    double m_minTime = 0.0;
    double m_maxTime = 10.0;
    double m_minValue = 0.0;
    double m_maxValue = 100.0;

    double m_globalReference = 0.0;
};

#endif // SENSORMODEL_H

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
};

class SensorModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { IdRole = Qt::UserRole + 1, NameRole, DataRole };

    explicit SensorModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void importFromTxt(const QString &fileUrl);
    Q_INVOKABLE void exportToCsv(const QString &fileUrl);
    Q_INVOKABLE void generateReport(int index);

    // Используем QAbstractSeries
    Q_INVOKABLE void fillSeries(QAbstractSeries *series, int sensorIndex, bool useCorrected, QString channel);

    bool parseTxtInternal(const QString &txtFilePath);

private:
    QVector<Sensor> m_sensors;
    QVariant sensorDataToVariantList(const Sensor &s) const;
    void preCalculateCalibration();
    static double safeDivide(double target, double current);
};

#endif // SENSORMODEL_H

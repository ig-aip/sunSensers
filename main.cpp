#include <QApplication>
#include <QQuickWidget>
#include <QQmlContext>
#include <QVBoxLayout>
#include <QWidget>
#include <QDir>
#include <QDebug>
#include "sensormodel.h"

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    SensorModel model;

    QString appPath = QCoreApplication::applicationDirPath();
    QString txtPath = QDir(appPath).filePath("results.txt"); // Ваш исходный файл
    QString jsonPath = QDir(appPath).filePath("sensors_gen.json"); // Будет создан

    // 1. Пробуем найти txt и сконвертировать
    if (QFile::exists(txtPath)) {
        qInfo() << "Found results.txt, converting to JSON...";
        model.convertTxtToJson(txtPath, jsonPath);
    } else {
        qWarning() << "results.txt not found at:" << txtPath;
        // Если txt нет, попробуем загрузить существующий json
    }

    // 2. Загружаем данные в модель
    model.loadFromJsonFile(jsonPath);

    // 3. Настройка UI
    QWidget window;
    window.setWindowTitle("Solar Telemetry Viewer");
    window.resize(1000, 700);

    QVBoxLayout *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);

    QQuickWidget *view = new QQuickWidget;
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    view->rootContext()->setContextProperty("sensorModel", &model);

    // Путь к QML файлу (убедитесь, что он лежит рядом с exe или используйте qrc)
    view->setSource(QUrl::fromLocalFile(QDir(appPath).filePath("main.qml")));

    layout->addWidget(view);
    window.show();

    return app.exec();
}

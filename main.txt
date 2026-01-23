#include <QApplication>
#include <QQuickWidget>
#include <QQmlContext>
#include <QVBoxLayout>
#include <QWidget>
#include <QDir>
#include <QDebug>
#include <QElapsedTimer>
#include <QQuickStyle>
#include <QFile>

// ВАЖНО: Подключаем заголовочный файл, а НЕ .cpp!
#include "sensormodel.h"

int main(int argc, char **argv) {
    // Устанавливаем стиль QML до создания QApplication
    QQuickStyle::setStyle("Basic");

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication app(argc, argv);

    // Таймер для замера скорости запуска
    QElapsedTimer timer;
    timer.start();

    qInfo() << "[0 ms] Program started.";

    // Создаем модель данных
    SensorModel model;

    // Определяем пути к файлам
    QString appPath = QCoreApplication::applicationDirPath();
    QString txtPath = QDir(appPath).filePath("results.txt");
    QString qmlPath = QDir(appPath).filePath("Main.qml");

    // Пытаемся автоматически загрузить данные при старте
    if (QFile::exists(txtPath)) {
        qInfo() << "[" << timer.elapsed() << "ms] results.txt found. Starting parsing...";

        // Вызываем внутренний метод парсинга
        bool ok = model.parseTxtInternal(txtPath);

        if (ok) {
            qInfo() << "[" << timer.elapsed() << "ms] Parsing & Math FINISHED. Data ready.";
        } else {
            qWarning() << "Parsing failed or file is empty.";
        }
    } else {
        qInfo() << "results.txt not found in:" << txtPath;
    }

    if (QFile::exists(txtPath)) {
        qInfo() << "File found at:" << txtPath;
        bool ok = model.parseTxtInternal(txtPath);
        if (ok) {
            qInfo() << "Loaded sensors count:" << model.rowCount();
        } else {
            qWarning() << "Parse failed! Check file format.";
        }
    } else {
        qWarning() << "CRITICAL: results.txt NOT FOUND at" << txtPath;
        qWarning() << "Current search folder:" << QDir::currentPath();
    }

    qInfo() << "[" << timer.elapsed() << "ms] Creating Window...";

    // Создаем обычное окно (QWidget), в которое положим QML
    QWidget window;
    window.setWindowTitle("Solar Sensor Analytics");
    window.resize(1100, 750);

    // Настраиваем layout, чтобы QML растягивался на все окно
    QVBoxLayout *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);

    // Создаем виджет для отображения QML
    QQuickWidget *view = new QQuickWidget;
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);

    // Связываем C++ модель с QML под именем "sensorModel"
    view->rootContext()->setContextProperty("sensorModel", &model);

    // Загружаем QML файл
    view->setSource(QUrl::fromLocalFile(qmlPath));

    // Добавляем QML виджет в окно
    layout->addWidget(view);

    // Показываем окно
    window.show();
    qInfo() << "[" << timer.elapsed() << "ms] Window shown.";

    return app.exec();
}

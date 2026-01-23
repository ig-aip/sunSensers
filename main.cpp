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

#include "sensormodel.h"

int main(int argc, char **argv) {
    QQuickStyle::setStyle("Basic");

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication app(argc, argv);
    QElapsedTimer timer;
    timer.start();

    SensorModel model;

    QString appPath = QCoreApplication::applicationDirPath();
    QString txtPath = QDir(appPath).filePath("results.txt");
    QString qmlPath = QDir(appPath).filePath("Main.qml");

    // Используем новый метод, который сделает TXT->JSON->Model
    if (model.loadResultsFile(txtPath)) {
        qInfo() << "[" << timer.elapsed() << "ms] Data pipeline (TXT->JSON->App) finished.";
    } else {
        qInfo() << "results.txt not found or failed to parse at:" << txtPath;
    }

    QWidget window;
    window.setWindowTitle("Solar Sensor Analytics (JSON Pipeline)");
    window.resize(1100, 750);

    QVBoxLayout *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);

    QQuickWidget *view = new QQuickWidget;
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    view->rootContext()->setContextProperty("sensorModel", &model);
    view->setSource(QUrl::fromLocalFile(qmlPath));

    layout->addWidget(view);
    window.show();

    return app.exec();
}

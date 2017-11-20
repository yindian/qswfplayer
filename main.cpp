#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "dumpgnashprovider.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QQuickImageProvider *swfBackend = new DumpGnashProvider(&app);
    engine.addImageProvider("swf", swfBackend);
    engine.rootContext()->setContextProperty("SwfDebug", SWF_DEBUG);
    engine.rootContext()->setContextProperty("SwfFps", SWF_FPS);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    return app.exec();
}

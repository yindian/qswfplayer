#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "dumpgnashprovider.h"

int main(int argc, char *argv[])
{
#if SWF_DEBUG
    qSetMessagePattern("[%{time process} %{function} %{line} %{threadid}] %{if-category}%{category}: %{endif}%{message}");
#endif
    QApplication app(argc, argv);

    QQmlApplicationEngine engine;
    DumpGnashProvider *swfBackend = new DumpGnashProvider;
    engine.addImageProvider("swf", swfBackend);
    engine.rootContext()->setContextProperty("SwfDebug", SWF_DEBUG);
    engine.rootContext()->setContextProperty("SwfFps", SWF_FPS);
    engine.rootContext()->setContextProperty("SwfBackend", swfBackend);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    return app.exec();
}

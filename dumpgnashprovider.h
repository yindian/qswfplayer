#ifndef DUMPGNASHPROVIDER_H
#define DUMPGNASHPROVIDER_H

#include <QObject>
#include <QQuickImageProvider>

#define SWF_WIDTH   1920
#define SWF_HEIGHT  1080
#define SWF_FPS     25

class DumpGnashProvider : public QObject, public QQuickImageProvider
{
    Q_OBJECT
public:
    explicit DumpGnashProvider(QObject *parent = 0);
    ~DumpGnashProvider();

signals:

public slots:
};

#endif // DUMPGNASHPROVIDER_H

#ifndef DUMPGNASHPROVIDER_H
#define DUMPGNASHPROVIDER_H

#include <QObject>
#include <QQuickImageProvider>

#define SWF_WIDTH   1920
#define SWF_HEIGHT  1080
#define SWF_FPS     25

class QProcess;

class DumpGnashProvider : public QObject, public QQuickImageProvider
{
    Q_OBJECT
public:
    explicit DumpGnashProvider(QObject *parent = 0);
    virtual QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize);
    ~DumpGnashProvider();

signals:

public slots:

protected slots:
    void slotFinished();
    void slotError();

private:
    void cleanUp();
    bool startDumpGnash();
    QProcess *m_pro;
    int m_frameIdx;
    int m_frameReq;
    QString m_fifo;
    QString m_swfFile;
    QImage m_frame;
};

#endif // DUMPGNASHPROVIDER_H

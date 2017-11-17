#ifndef DUMPGNASHPROVIDER_H
#define DUMPGNASHPROVIDER_H

#include <QObject>
#include <QQuickImageProvider>
#include <QElapsedTimer>

#if 0
#define SWF_WIDTH   960
#define SWF_HEIGHT  540
#else
#define SWF_WIDTH   1920
#define SWF_HEIGHT  1080
#endif
#define SWF_FPS     25

class QFile;
class QProcess;
class QTcpSocket;

class DumpGnashProvider : public QObject, public QQuickImageProvider
{
    Q_OBJECT
public:
    explicit DumpGnashProvider(QObject *parent = 0);
    virtual QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize);
    ~DumpGnashProvider();

signals:
    void signalFrameData(int frameIdx, QByteArray buf);

public slots:

protected slots:
    void slotFinished();
    void slotError();
    void slotReadyRead();
    void slotFrameData(int frameIdx, QByteArray buf);

private:
    void cleanUp();
    bool startDumpGnash();
    QProcess *m_pro;
    int m_frameIdx;
    int m_frameReq;
    QFile *m_fifo;
    QTcpSocket *m_fifoSkt;
    QString m_swfFile;
    QImage m_frame;
    QByteArray m_buf;
    QElapsedTimer m_timer;
};

#endif // DUMPGNASHPROVIDER_H

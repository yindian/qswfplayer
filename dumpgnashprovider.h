#ifndef DUMPGNASHPROVIDER_H
#define DUMPGNASHPROVIDER_H

#include <QObject>
#include <QQuickImageProvider>
#include <QElapsedTimer>
#include <QSemaphore>

#define SWF_DEBUG 1
#define SWF_AUDIO
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
#ifdef SWF_AUDIO
    void slotReadyReadAudio();
#endif
    void slotFrameData(int frameIdx, QByteArray buf);
    void slotStartDumpGnash(QString uri, int frameReq);
    void slotContinueDumpGnash(int frameReq);

private:
    void cleanUp();
    bool startDumpGnash();
    bool stopDumpGnash();
    bool contDumpGnash();
    bool isDumpGnashStopped();
    QProcess *m_pro;
    int m_frameIdx;
    int m_frameReq;
    bool m_stopped;
    QFile *m_fifo;
    QTcpSocket *m_fifoSkt;
#ifdef SWF_AUDIO
    QFile *m_audFifo;
    QTcpSocket *m_audFifoSkt;
    QByteArray m_bufAudio;
#endif
    QString m_swfFile;
    QImage m_frame;
    QByteArray m_frameBuf;
    QByteArray m_buf;
    QElapsedTimer m_timer;
    QSemaphore m_sema;
};

#endif // DUMPGNASHPROVIDER_H

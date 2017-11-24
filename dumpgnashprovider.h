#ifndef DUMPGNASHPROVIDER_H
#define DUMPGNASHPROVIDER_H

#include <QObject>
#include <QQuickImageProvider>
#include <QElapsedTimer>
#include <QSemaphore>
#include <QTimer>

#define SWF_DEBUG 0
#define SWF_AUDIO
#ifdef SWF_AUDIO
#include <QAudio>
#include <QAudioFormat>
#endif

#if 0
#define SWF_WIDTH   960
#define SWF_HEIGHT  540
#else
#define SWF_WIDTH   1920
#define SWF_HEIGHT  1080
#endif
#define SWF_FPS     24

class QAudioOutput;
class QFile;
class QProcess;
class QTcpSocket;

class DumpGnashProvider : public QObject, public QQuickImageProvider
{
    Q_OBJECT
public:
    explicit DumpGnashProvider(QObject *parent = 0);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) Q_DECL_OVERRIDE;
    bool eventFilter(QObject *obj, QEvent *event) Q_DECL_OVERRIDE;
    ~DumpGnashProvider() Q_DECL_OVERRIDE;

signals:
    void signalFrameData(int frameIdx, QByteArray buf);

public slots:

protected slots:
    void slotFinished();
    void slotError();
    void slotSocketError();
    void slotReadyRead();
    void parseVideo();
#ifdef SWF_AUDIO
    void slotReadyReadAudio();
    void slotPushAudio();
    void slotNewAudioState(QAudio::State state);
#endif
    void slotFrameData(int frameIdx, QByteArray buf);
    void slotStartDumpGnash(QString uri, int frameReq);
    void slotContinueDumpGnash(int frameReq);
    void cleanUp();
    void generateFrame();

private:
#ifdef SWF_AUDIO
    bool prepareAudioOutput();
#endif
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
    int m_audioState;
    QAudioOutput *m_audioOutput;
    QIODevice *m_feed;
    QByteArray m_bufAudio;
    QAudioFormat m_audioFormat;
    QTimer m_audioTimer;
#endif
    QString m_swfFile;
    QImage m_frame;
    QByteArray m_frameBuf;
    QByteArray m_buf;
    QElapsedTimer m_timer;
    QSemaphore m_sema;
};

#endif // DUMPGNASHPROVIDER_H

#include "dumpgnashprovider.h"
#include <QAudioOutput>
#include <QDebug>
#include <QProcess>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QtEndian>
#include <signal.h>
#include <sys/stat.h>

#define VERIFY_OR_BREAK_AND(_cond, _and) ({if (!(_cond)) { qDebug() << #_cond << "failed on" << __FILE__ << ":" << __LINE__; _and; break;}})
#define VERIFY_OR_BREAK(_cond) VERIFY_OR_BREAK_AND(_cond, (void)NULL)

#define SHORT_WAIT_ITVL 100
#define LONG_WAIT_ITVL  500

#ifdef SWF_AUDIO
#define STATE_WAV_HEADER    0
#define STATE_WAV_DATA      1

#define WAVE_FORMAT_UNKNOWN 0x0000
#define WAVE_FORMAT_PCM     0x0001
#endif

DumpGnashProvider::DumpGnashProvider(QObject *parent) : QObject(parent)
  , QQuickImageProvider(QQuickImageProvider::Image, QQuickImageProvider::ForceAsynchronousImageLoading)
  , m_pro(NULL)
  , m_frameIdx(0)
  , m_frameReq(-1)
  , m_stopped(false)
  , m_fifo(NULL)
  , m_fifoSkt(NULL)
  #ifdef SWF_AUDIO
  , m_audFifo(NULL)
  , m_audFifoSkt(NULL)
  , m_audioState(STATE_WAV_HEADER)
  , m_audioOutput(NULL)
  , m_feed(NULL)
  #endif
  , m_sema(1)
{
    connect(this, SIGNAL(signalFrameData(int,QByteArray)), this, SLOT(slotFrameData(int,QByteArray)));
}

QImage DumpGnashProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    static const QSize sz(SWF_WIDTH, SWF_HEIGHT);
    int pos = id.lastIndexOf('/');
    Q_ASSERT(pos > 0);
    QString uri = id.left(pos);
#if SWF_DEBUG
    qDebug() << id << uri << id.mid(pos + 1);
#endif
    bool ok = false;
    Q_UNUSED(ok);
    int idx = id.mid(pos + 1).toInt(&ok);
    Q_ASSERT(ok);
    do
    {
        VERIFY_OR_BREAK(m_sema.tryAcquire());
        if (m_pro && m_swfFile == uri && m_frameIdx <= idx)
        {
            QMetaObject::invokeMethod(this, "slotContinueDumpGnash", Qt::QueuedConnection,
                                      Q_ARG(int, idx));
        }
        else if (m_pro && m_swfFile == uri && m_frameIdx == idx + 1)
        {
            m_sema.release();
        }
        else
        {
            QMetaObject::invokeMethod(this, "slotStartDumpGnash", Qt::QueuedConnection,
                                      Q_ARG(QString, uri),
                                      Q_ARG(int, idx));
        }
        m_sema.acquire();
        m_sema.release();
        if (size)
        {
            *size = sz;
        }
        if (requestedSize.isValid() && !m_frame.isNull())
        {
            return m_frame.scaled(requestedSize);
        }
        return m_frame;
    } while (0);
    return QImage();
}

DumpGnashProvider::~DumpGnashProvider()
{
    cleanUp();
}

void DumpGnashProvider::slotFinished()
{
    QProcess *pro = qobject_cast<QProcess *>(sender());
    Q_ASSERT(pro);
    qDebug() << "exit" << "status" << pro->exitStatus() << "code" << pro->exitCode() << "elapsed" << m_timer.elapsed() << "ms";
    m_timer.invalidate();
    m_frame = QImage();
#ifdef SWF_AUDIO
    QFile f("/tmp/record.wav");
    if (f.open(QIODevice::WriteOnly))
    {
        f.write(m_bufAudio);
        f.close();
    }
#endif
    m_sema.release();
}

void DumpGnashProvider::slotError()
{
    QProcess *pro = qobject_cast<QProcess *>(sender());
    Q_ASSERT(pro);
    qDebug() << "!!!" << pro->errorString();
    if (pro->error() == QProcess::FailedToStart)
    {
        qDebug() << "failed to start" << pro->program();
    }
}

void DumpGnashProvider::slotReadyRead()
{
    QIODevice *in = qobject_cast<QIODevice *>(sender());
    Q_ASSERT(in);
#if SWF_DEBUG
//    qDebug() << "got" << in->bytesAvailable() << "bytes";
#endif
    if (m_sema.available())
    {
#if SWF_DEBUG
        qDebug() << "frame" << m_frameIdx << "req" << m_frameReq << "available";
#endif
        return;
    }
    m_buf.append(in->readAll());
    static const int frameSize = SWF_WIDTH * SWF_HEIGHT * 4;
    while (m_buf.size() >= frameSize && m_frameIdx <= m_frameReq)
    {
        QByteArray ba = m_buf.mid(frameSize);
        m_buf.truncate(frameSize);
        emit signalFrameData(m_frameIdx++, m_buf);
        if (m_frameIdx > m_frameReq)
        {
            m_frameBuf = m_buf;
            m_frame = QImage((const uchar *) m_frameBuf.constData(), SWF_WIDTH, SWF_HEIGHT, QImage::Format_ARGB32_Premultiplied);
            m_sema.release();
            stopDumpGnash();
        }
        m_buf = ba;
    }
}

#ifdef SWF_AUDIO
void DumpGnashProvider::slotReadyReadAudio()
{
    QIODevice *in = qobject_cast<QIODevice *>(sender());
    Q_ASSERT(in);
#if SWF_DEBUG
//    qDebug() << "got" << in->bytesAvailable() << "bytes";
#endif
    m_bufAudio.append(in->readAll());
    if (m_audioState == STATE_WAV_HEADER)
    {
        if (prepareAudioOutput())
        {
#if SWF_DEBUG
            qDebug() << "wave header read";
#endif
            m_audioState = STATE_WAV_DATA;
        }
    }
    if (m_audioState == STATE_WAV_DATA && m_audioOutput->bytesFree())
    {
        int written = m_feed->write(m_bufAudio);
        if (written > 0)
        {
            m_bufAudio = m_bufAudio.mid(written);
        }
    }
}
#endif

void DumpGnashProvider::slotFrameData(int frameIdx, QByteArray buf)
{
#if SWF_DEBUG
    qDebug() << "got" << "frame" << frameIdx;
#endif
    Q_UNUSED(frameIdx);
    Q_UNUSED(buf);
}

void DumpGnashProvider::slotStartDumpGnash(QString uri, int frameReq)
{
    Q_ASSERT(m_sema.available() == 0);
    if (m_pro)
    {
        cleanUp();
        QMetaObject::invokeMethod(this, "slotStartDumpGnash", Qt::QueuedConnection,
                                  Q_ARG(QString, uri),
                                  Q_ARG(int, frameReq));
        return;
    }
    m_swfFile = uri;
    m_frameReq = frameReq;
    if (!startDumpGnash())
    {
        qDebug() << "failed to start";
    }
}

void DumpGnashProvider::slotContinueDumpGnash(int frameReq)
{
    Q_ASSERT(m_sema.available() == 0);
    m_frameReq = frameReq;
    if (!contDumpGnash())
    {
        qDebug() << "failed to resume";
    }
}

void DumpGnashProvider::cleanUp()
{
    if (m_pro)
    {
        if (m_pro->processId())
        {
            ::kill(m_pro->processId(), SIGINT);
            ::kill(m_pro->processId(), SIGINT);
            if (!m_pro->waitForFinished(SHORT_WAIT_ITVL))
            {
                m_pro->kill();
                if (!m_pro->waitForFinished(LONG_WAIT_ITVL))
                {
                    m_pro->terminate();
                    m_pro->waitForFinished();
                }
            }
        }
        m_pro->disconnect();
        m_pro->deleteLater();
        m_pro = NULL;
    }
    if (m_fifo)
    {
        m_fifo->remove();
        m_fifo->deleteLater();
        m_fifo = NULL;
    }
    if (m_fifoSkt)
    {
        m_fifoSkt->disconnect();
        m_fifoSkt->deleteLater();
        m_fifoSkt = NULL;
    }
#ifdef SWF_AUDIO
    if (m_audFifo)
    {
        m_audFifo->remove();
        m_audFifo->deleteLater();
        m_audFifo = NULL;
    }
    if (m_audFifoSkt)
    {
        m_audFifoSkt->disconnect();
        m_audFifoSkt->deleteLater();
        m_audFifoSkt = NULL;
    }
    m_audioState = STATE_WAV_HEADER;
    if (m_audioOutput)
    {
        m_audioOutput->deleteLater();
        m_audioOutput = NULL;
    }
    m_feed = NULL; // owned by audio output
    m_bufAudio.clear();
#endif
    m_swfFile.clear();
    m_stopped = false;
    m_frameIdx = 0;
    m_frameReq = -1;
    m_frame = QImage();
    m_frameBuf.clear();
    m_buf.clear();
    m_timer.invalidate();
}

#ifdef SWF_AUDIO
bool DumpGnashProvider::prepareAudioOutput()
{
    do
    {
        VERIFY_OR_BREAK(m_bufAudio.startsWith("RIFF"));
        VERIFY_OR_BREAK(m_bufAudio.mid(8, 8) == "WAVEfmt ");
        int pos = 16;
        VERIFY_OR_BREAK(m_bufAudio.size() >= pos + 4/*len*/ + 16/*fmt fields*/ + 4/*data*/ + 4/*len*/);
#define READ16_N_ADVANCE    ({quint16 ret = qFromLittleEndian<quint16>((const uchar *) m_bufAudio.constData() + pos); pos += 2; ret;})
#define READ32_N_ADVANCE    ({quint32 ret = qFromLittleEndian<quint32>((const uchar *) m_bufAudio.constData() + pos); pos += 4; ret;})
        quint32 len = READ32_N_ADVANCE;
        VERIFY_OR_BREAK(len >= 16);
        int oldPos = pos;
        quint16 wFormatTag = READ16_N_ADVANCE;
        VERIFY_OR_BREAK(wFormatTag == WAVE_FORMAT_PCM || wFormatTag == WAVE_FORMAT_UNKNOWN);
        m_audioFormat.setByteOrder(QAudioFormat::LittleEndian);
        m_audioFormat.setCodec("audio/pcm");
        m_audioFormat.setChannelCount(READ16_N_ADVANCE);
        m_audioFormat.setSampleRate(READ32_N_ADVANCE);
        quint32 nAvgBytesPerSec = READ32_N_ADVANCE;
        Q_UNUSED(nAvgBytesPerSec);
        quint16 nBlockAlign = READ16_N_ADVANCE;
        Q_UNUSED(nBlockAlign);
        m_audioFormat.setSampleSize(READ16_N_ADVANCE);
        m_audioFormat.setSampleType(m_audioFormat.sampleSize() == 8 ? QAudioFormat::UnSignedInt : QAudioFormat::SignedInt);
        qDebug() << m_audioFormat;
        pos = oldPos + len;
        while (m_bufAudio.mid(pos, 4) != "data")
        {
            VERIFY_OR_BREAK(m_bufAudio.size() > pos + 8);
            qDebug() << "skipping" << m_bufAudio.mid(pos, 4);
            pos += 4;
            len = READ32_N_ADVANCE;
            pos += len;
        }
        VERIFY_OR_BREAK(m_bufAudio.mid(pos, 4) == "data");
        pos += 8; // skip len
        VERIFY_OR_BREAK(m_bufAudio.size() >= pos);
#if SWF_DEBUG
        qDebug() << "got audio data";
#endif
        m_bufAudio = m_bufAudio.mid(pos);
        if (m_audioOutput)
        {
            delete m_audioOutput;
        }
        m_audioOutput = new QAudioOutput(m_audioFormat, this);
        m_feed = m_audioOutput->start();
        return true;
    } while (0);
    return false;
}
#endif

bool DumpGnashProvider::startDumpGnash()
{
    do
    {
        QTemporaryFile tmpFile;
        VERIFY_OR_BREAK(tmpFile.open());
        QString fifo = tmpFile.fileName();
        tmpFile.close();
        tmpFile.remove();
#if SWF_DEBUG
        qDebug() << fifo;
#endif
        VERIFY_OR_BREAK_AND(mkfifo(fifo.toLocal8Bit().constData(), 0666) == 0, perror("mkfifo"));
#if SWF_DEBUG
        qDebug() << "fifo" << fifo << "created";
#endif
#ifdef SWF_AUDIO
        static const QString dotAudio = QStringLiteral(".audio");
        VERIFY_OR_BREAK_AND(mkfifo((fifo + dotAudio).toLocal8Bit().constData(), 0666) == 0, perror("mkfifo"));
#if SWF_DEBUG
        qDebug() << "fifo" << (fifo + dotAudio) << "created";
#endif
#endif
        m_pro = new QProcess(this);
        m_pro->setProcessChannelMode(QProcess::ForwardedChannels);
        connect(m_pro, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotFinished()));
        connect(m_pro, SIGNAL(error(QProcess::ProcessError)), this, SLOT(slotError()));
        QStringList args;
#ifdef SWF_AUDIO
        args << "-1" << "-r" << "3";
#else
        args << "-1" << "-r" << "1";
#endif
//        args << "-v" << "-a" << "-p";
        args << "-j" << QString::number(SWF_WIDTH);
        args << "-k" << QString::number(SWF_HEIGHT);
        args << "-D" << QStringLiteral("%1@%2").arg(fifo).arg(SWF_FPS);
#ifdef SWF_AUDIO
        args << "-A" << (fifo + dotAudio);
#endif
        args << m_swfFile;
        m_pro->start("dump-gnash", args);
#if SWF_DEBUG
        qDebug() << "dump-gnash" << args;
#endif
        VERIFY_OR_BREAK(m_pro->waitForStarted(LONG_WAIT_ITVL));
        m_timer.start();
        m_fifo = new QFile(fifo, this);
        VERIFY_OR_BREAK(m_fifo->open(QFile::ReadOnly));
#if SWF_DEBUG
        qDebug() << "fifo" << fifo << "opened";
#endif
        m_fifoSkt = new QTcpSocket(this);
        VERIFY_OR_BREAK(m_fifoSkt->setSocketDescriptor(m_fifo->handle(), QAbstractSocket::ConnectedState, QIODevice::ReadOnly));
        connect(m_fifoSkt, SIGNAL(readyRead()), this, SLOT(slotReadyRead()));
#if SWF_DEBUG
        qDebug() << "socket" << m_fifoSkt->socketDescriptor()<< "connected";
#endif
#ifdef SWF_AUDIO
        fifo += dotAudio;
        m_audFifo = new QFile(fifo, this);
        VERIFY_OR_BREAK(m_audFifo->open(QFile::ReadOnly));
#if SWF_DEBUG
        qDebug() << "fifo" << fifo << "opened";
#endif
        m_audFifoSkt = new QTcpSocket(this);
        VERIFY_OR_BREAK(m_audFifoSkt->setSocketDescriptor(m_audFifo->handle(), QAbstractSocket::ConnectedState, QIODevice::ReadOnly));
        connect(m_audFifoSkt, SIGNAL(readyRead()), this, SLOT(slotReadyReadAudio()));
#if SWF_DEBUG
        qDebug() << "socket" << m_audFifoSkt->socketDescriptor()<< "connected";
#endif
#endif
        return true;
    } while (0);
    cleanUp();
    m_sema.release();
    return false;
}

bool DumpGnashProvider::stopDumpGnash()
{
    if (isDumpGnashStopped())
    {
        qDebug() << "already stopped";
        return true;
    }
    if (!m_pro->processId())
    {
        qDebug() << "not started";
        return false;
    }
    m_stopped = true;
    ::kill(m_pro->processId(), SIGSTOP);
#if SWF_DEBUG
    qDebug() << "stopped";
#endif
    return true;
}

bool DumpGnashProvider::contDumpGnash()
{
    if (!isDumpGnashStopped())
    {
        qDebug() << "not stopped";
        return true;
    }
    if (!m_pro->processId())
    {
        qDebug() << "not started";
        return false;
    }
    m_stopped = false;
    ::kill(m_pro->processId(), SIGCONT);
#if SWF_DEBUG
    qDebug() << "continued";
#endif
    return true;
}

bool DumpGnashProvider::isDumpGnashStopped()
{
    return m_stopped;
}


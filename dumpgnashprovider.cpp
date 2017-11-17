#include "dumpgnashprovider.h"
#include <QDebug>
#include <QProcess>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <signal.h>
#include <sys/stat.h>

#define VERIFY_OR_BREAK_AND(_cond, _and) ({if (!(_cond)) { qDebug() << #_cond << "failed on" << __FILE__ << ":" << __LINE__; _and; break;}})
#define VERIFY_OR_BREAK(_cond) VERIFY_OR_BREAK_AND(_cond, (void)NULL)

#define SHORT_WAIT_ITVL 100
#define LONG_WAIT_ITVL  500

DumpGnashProvider::DumpGnashProvider(QObject *parent) : QObject(parent)
  , QQuickImageProvider(QQuickImageProvider::Image, QQuickImageProvider::ForceAsynchronousImageLoading)
  , m_pro(NULL)
  , m_frameIdx(0)
  , m_frameReq(-1)
  , m_stopped(false)
  , m_fifo(NULL)
  , m_fifoSkt(NULL)
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
    qDebug() << id << uri << id.mid(pos + 1);
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
    if (!in)
    {
        in = m_fifoSkt;
    }
//    qDebug() << "got" << in->bytesAvailable() << "bytes";
    if (m_sema.available())
    {
        qDebug() << "frame" << m_frameIdx << "req" << m_frameReq << "available";
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

void DumpGnashProvider::slotFrameData(int frameIdx, QByteArray buf)
{
    qDebug() << "got" << "frame" << frameIdx;
    Q_UNUSED(buf);
}

void DumpGnashProvider::slotStartDumpGnash(QString uri, int frameReq)
{
    Q_ASSERT(m_sema.available() == 0);
    cleanUp();
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
    m_swfFile.clear();
    m_stopped = false;
    m_frameIdx = 0;
    m_frameReq = -1;
    m_frame = QImage();
    m_frameBuf.clear();
    m_buf.clear();
    m_timer.invalidate();
}

bool DumpGnashProvider::startDumpGnash()
{
    do
    {
        QTemporaryFile tmpFile;
        VERIFY_OR_BREAK(tmpFile.open());
        QString fifo = tmpFile.fileName();
        tmpFile.close();
        tmpFile.remove();
        qDebug() << fifo;
        VERIFY_OR_BREAK_AND(mkfifo(fifo.toLocal8Bit().constData(), 0666) == 0, perror("mkfifo"));
        qDebug() << "fifo" << fifo << "created";
        m_pro = new QProcess(this);
        m_pro->setProcessChannelMode(QProcess::ForwardedChannels);
        connect(m_pro, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotFinished()));
        connect(m_pro, SIGNAL(error(QProcess::ProcessError)), this, SLOT(slotError()));
        QStringList args;
        args << "-1" << "-r" << "1";
        args << "-j" << QString::number(SWF_WIDTH);
        args << "-k" << QString::number(SWF_HEIGHT);
        args << "-D" << QStringLiteral("%1@%2").arg(fifo).arg(SWF_FPS);
        args << m_swfFile;
        m_pro->start("dump-gnash", args);
        qDebug() << "dump-gnash" << args;
        VERIFY_OR_BREAK(m_pro->waitForStarted(LONG_WAIT_ITVL));
        m_timer.start();
        m_fifo = new QFile(fifo, this);
        VERIFY_OR_BREAK(m_fifo->open(QFile::ReadOnly));
        qDebug() << "fifo" << fifo << "opened";
        m_fifoSkt = new QTcpSocket(this);
        VERIFY_OR_BREAK(m_fifoSkt->setSocketDescriptor(m_fifo->handle(), QAbstractSocket::ConnectedState, QIODevice::ReadOnly));
        connect(m_fifoSkt, SIGNAL(readyRead()), this, SLOT(slotReadyRead()));
        qDebug() << "socket" << m_fifoSkt->socketDescriptor()<< "connected";
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
    qDebug() << "stopped";
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
    qDebug() << "continued";
    return true;
}

bool DumpGnashProvider::isDumpGnashStopped()
{
    return m_stopped;
}


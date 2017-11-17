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
  , QQuickImageProvider(QQuickImageProvider::Image/*, QQuickImageProvider::ForceAsynchronousImageLoading*/)
  , m_pro(NULL)
  , m_frameIdx(0)
  , m_frameReq(-1)
  , m_fifo(NULL)
  , m_fifoSkt(NULL)
{
    connect(this, SIGNAL(signalFrameData(int,QByteArray)), this, SLOT(slotFrameData(int,QByteArray)));
}

QImage DumpGnashProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QSize sz(SWF_WIDTH, SWF_HEIGHT);
    if (requestedSize.isValid())
    {
        sz = requestedSize;
    }
    int pos = id.lastIndexOf('/');
    Q_ASSERT(pos > 0);
    QString uri = id.left(pos);
    qDebug() << id << uri << id.mid(pos + 1);
    bool ok = false;
    Q_UNUSED(ok);
    int idx = id.mid(pos + 1).toInt(&ok);
    Q_ASSERT(ok);
    if (m_pro && m_swfFile == uri && m_frameIdx <= idx)
    {
        return m_frame;
    }
    cleanUp();
    m_swfFile = uri;
    do
    {
        VERIFY_OR_BREAK(startDumpGnash());
        if (size)
        {
            *size = sz;
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
    Q_ASSERT(in);
//    qDebug() << "got" << in->bytesAvailable() << "bytes";
    m_buf.append(in->readAll());
    static const int frameSize = SWF_WIDTH * SWF_HEIGHT * 4;
    while (m_buf.size() >= frameSize)
    {
        QByteArray ba = m_buf.mid(frameSize);
        m_buf.truncate(frameSize);
        emit signalFrameData(m_frameIdx++, m_buf);
        m_buf = ba;
    }
}

void DumpGnashProvider::slotFrameData(int frameIdx, QByteArray buf)
{
    qDebug() << "got" << "frame" << frameIdx;
    Q_UNUSED(buf);
}

void DumpGnashProvider::cleanUp()
{
    if (m_pro)
    {
        if (m_pro->processId())
        {
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
    m_frameIdx = 0;
    m_frameReq = -1;
    m_frame = QImage();
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
        m_pro = new QProcess();
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
        m_fifo = new QFile(fifo);
        VERIFY_OR_BREAK(m_fifo->open(QFile::ReadOnly));
        qDebug() << "fifo" << fifo << "opened";
        m_fifoSkt = new QTcpSocket();
        VERIFY_OR_BREAK(m_fifoSkt->setSocketDescriptor(m_fifo->handle(), QAbstractSocket::ConnectedState, QIODevice::ReadOnly));
        connect(m_fifoSkt, SIGNAL(readyRead()), this, SLOT(slotReadyRead()));
        qDebug() << "socket" << m_fifoSkt->socketDescriptor()<< "connected";
        return true;
    } while (0);
    cleanUp();
    return false;
}


#include "dumpgnashprovider.h"
#include <QDebug>
#include <QProcess>
#include <QTemporaryFile>
#include <signal.h>
#include <sys/stat.h>

#define VERIFY_OR_BREAK_AND(_cond, _and) ({if (!(_cond)) { qDebug() << #_cond << "failed on" << __FILE__ << ":" << __LINE__; _and; break;}})
#define VERIFY_OR_BREAK(_cond) VERIFY_OR_BREAK_AND(_cond, (void)NULL)

DumpGnashProvider::DumpGnashProvider(QObject *parent) : QObject(parent)
  , QQuickImageProvider(QQuickImageProvider::Image)
  , m_pro(NULL)
  , m_frameIdx(0)
  , m_frameReq(-1)
{

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
    qDebug() << "exit" << "status" << pro->exitStatus() << "code" << pro->exitCode();
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

void DumpGnashProvider::cleanUp()
{
    if (m_pro)
    {
        if (m_pro->processId())
        {
            ::kill(m_pro->processId(), SIGINT);
            if (!m_pro->waitForFinished(500))
            {
                m_pro->kill();
                if (!m_pro->waitForFinished(500))
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
    if (!m_fifo.isEmpty())
    {
        QFile::remove(m_fifo);
        m_fifo.clear();
    }
    m_swfFile.clear();
    m_frameIdx = 0;
    m_frameReq = -1;
}

bool DumpGnashProvider::startDumpGnash()
{
    do
    {
        QTemporaryFile tmpFile;
        VERIFY_OR_BREAK(tmpFile.open());
        m_fifo = tmpFile.fileName();
        tmpFile.close();
        tmpFile.remove();
        qDebug() << m_fifo;
        VERIFY_OR_BREAK_AND(mkfifo(m_fifo.toLocal8Bit().constData(), 0777) == 0, perror("mkfifo"));
        qDebug() << "fifo" << m_fifo << "created";
        m_pro = new QProcess(this);
        m_pro->setProcessChannelMode(QProcess::ForwardedChannels);
        connect(m_pro, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotFinished()));
        connect(m_pro, SIGNAL(error(QProcess::ProcessError)), this, SLOT(slotError()));
        QStringList args;
        args << "-1" << "-r" << "1";
        args << "-j" << QString::number(SWF_WIDTH);
        args << "-k" << QString::number(SWF_HEIGHT);
        args << "-D" << QStringLiteral("%1@%2").arg(m_fifo).arg(SWF_FPS);
        args << m_swfFile;
        m_pro->start("dump-gnash", args);
        qDebug() << "dump-gnash" << args;
        return true;
    } while (0);
    return false;
}


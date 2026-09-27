#include "logger.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>

static QFile *g_logFile = nullptr;
static QMutex g_mutex;
static constexpr qint64 MAX_LOG_SIZE = 5 * 1024 * 1024;

void Logger::init(const QString &logFilePath) {
    QMutexLocker lk(&g_mutex);
    if (g_logFile) {
        g_logFile->close();
        delete g_logFile;
    }

    if (QFileInfo(logFilePath).size() >= MAX_LOG_SIZE) {
        const QString previousLogPath = logFilePath + ".previous";
        QFile::remove(previousLogPath);
        QFile::rename(logFilePath, previousLogPath);
    }

    g_logFile = new QFile(logFilePath);
    static_cast<void>(g_logFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append));
}

void Logger::log(const QString &msg) {
    QMutexLocker lk(&g_mutex);
    if (!g_logFile || !g_logFile->isOpen()) return;
    QTextStream ts(g_logFile);
    const auto t = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ts << t << "  " << msg << "\r\n";
    g_logFile->flush();
}

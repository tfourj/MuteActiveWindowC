#include "logger.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>

static QFile *g_logFile = nullptr;
static QMutex g_mutex;

void Logger::init(const QString &logFilePath) {
    QMutexLocker lk(&g_mutex);
    if (g_logFile) {
        g_logFile->close();
        delete g_logFile;
    }
    g_logFile = new QFile(logFilePath);
    g_logFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);
}

void Logger::log(const QString &msg) {
    QMutexLocker lk(&g_mutex);
    if (!g_logFile || !g_logFile->isOpen()) return;
    QTextStream ts(g_logFile);
    const auto t = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ts << t << "  " << msg << "\r\n";
    g_logFile->flush();
}

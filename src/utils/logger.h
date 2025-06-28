#pragma once
#include <QString>
class Logger {
public:
    static void init(const QString &logFilePath);
    static void log(const QString &msg);
};

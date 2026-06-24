#include "Logging.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include <windows.h>

namespace {

QFile* g_logFile = nullptr;
QMutex g_logMutex;
bool g_consoleAttached = false;

QString messageTypeName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARN");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }

    return QStringLiteral("LOG");
}

void writeConsoleLine(const QString& line)
{
    if (!g_consoleAttached) {
        return;
    }

    const QString text = line + QStringLiteral("\r\n");
    DWORD written = 0;
    const auto wide = text.toStdWString();
    WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), wide.c_str(), static_cast<DWORD>(wide.size()), &written, nullptr);
}

void phoenixMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    QString line = QStringLiteral("[%1] [%2] %3")
                       .arg(timestamp, messageTypeName(type), msg);

    if (context.file && *context.file) {
        line += QStringLiteral(" (%1:%2)").arg(QString::fromLocal8Bit(context.file)).arg(context.line);
    }

    {
        QMutexLocker locker(&g_logMutex);
        if (g_logFile && g_logFile->isOpen()) {
            QTextStream stream(g_logFile);
            stream << line << '\n';
            stream.flush();
        }
    }

    OutputDebugStringW((line + QStringLiteral("\n")).toStdWString().c_str());
    writeConsoleLine(line);

    if (type == QtFatalMsg) {
        abort();
    }
}

} // namespace

namespace Logging {

void initialize()
{
    if (!g_consoleAttached) {
        g_consoleAttached = AttachConsole(ATTACH_PARENT_PROCESS) != FALSE;
        if (g_consoleAttached) {
            SetConsoleOutputCP(CP_UTF8);
        }
    }

    const QString logPath = logFilePath();
    QDir().mkpath(QFileInfo(logPath).absolutePath());

    static QFile file(logPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        g_logFile = &file;
    }

    qInstallMessageHandler(phoenixMessageHandler);
}

QString logFilePath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("PhoenixShell.log"));
}

} // namespace Logging

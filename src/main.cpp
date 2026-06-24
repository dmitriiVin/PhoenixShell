#include "Logging.h"
#include "ShellMainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("PhoenixShell"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    Logging::initialize();
    qInfo().noquote() << QStringLiteral("PhoenixShell starting");

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("PhoenixShell WinPE desktop shell"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption windowedOption(QStringLiteral("windowed"), QStringLiteral("Run in a normal window for testing."));
    parser.addOption(windowedOption);
    parser.process(app);

    ShellMainWindow shell;
    if (parser.isSet(windowedOption)) {
        shell.setWindowFlags(Qt::Window);
        shell.resize(1280, 720);
        shell.show();
        qInfo().noquote() << QStringLiteral("PhoenixShell running in windowed test mode");
    } else {
        shell.showFullScreen();
        qInfo().noquote() << QStringLiteral("PhoenixShell running fullscreen");
    }

    const int exitCode = app.exec();
    qInfo().noquote() << QStringLiteral("PhoenixShell exiting code=%1").arg(exitCode);
    return exitCode;
}

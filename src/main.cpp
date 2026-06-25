#include "ApplicationBootstrapper.h"
#include "Logging.h"
#include "ShellMainWindow.h"
#include "ToolRegistry.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <memory>

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

    // Шаг 1: Инициализировать ToolRegistry
    auto toolRegistry = std::make_unique<ToolRegistry>();

    // Шаг 2: Инициализировать ApplicationBootstrapper
    auto bootstrapper = std::make_unique<ApplicationBootstrapper>();

    // Подключить сигналы загрузчика приложения для отладки
    QObject::connect(bootstrapper.get(), &ApplicationBootstrapper::initializationProgress,
                     [](const QString& message) {
                         qInfo().noquote() << QStringLiteral("[INIT] %1").arg(message);
                     });

    QObject::connect(bootstrapper.get(), &ApplicationBootstrapper::initializationError,
                     [](const QString& errorMessage) {
                         qWarning().noquote() << QStringLiteral("[INIT ERROR] %1").arg(errorMessage);
                     });

    // Шаг 3: Выполнить инициализацию
    bool initSuccess = bootstrapper->initialize(toolRegistry.get());

    if (!initSuccess) {
        qCritical().noquote() << QStringLiteral("Application bootstrap failed");
        // Можно продолжить с локальной конфигурацией
        // return 1;
    }

    // Шаг 4: Создать и показать главное окно
    ShellMainWindow shell;

    // Передать регистр инструментов в главное окно (если это необходимо)
    // shell.setToolRegistry(toolRegistry.get());

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

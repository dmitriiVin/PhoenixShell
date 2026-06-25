#include "ApplicationBootstrapper.h"

#include "PartitionScanner.h"
#include "DataLoader.h"
#include "ToolRegistry.h"

#include <QDebug>
#include <QSysInfo>
#include <windows.h>

ApplicationBootstrapper::ApplicationBootstrapper(QObject* parent)
    : QObject(parent)
{
}

ApplicationBootstrapper::~ApplicationBootstrapper() = default;

bool ApplicationBootstrapper::initialize(ToolRegistry* toolRegistry)
{
    if (!toolRegistry) {
        logError(QStringLiteral("ToolRegistry is null"));
        emit initializationError(QStringLiteral("Critical: ToolRegistry is null"));
        return false;
    }

    logProgress(QStringLiteral("Starting PhoenixShell initialization..."));

    // Шаг 1: Определить окружение
    logProgress(QStringLiteral("Detecting environment..."));
    bool isWinPE = isWinPEEnvironment();
    qInfo().noquote() << QStringLiteral("Environment: %1").arg(isWinPE ? QStringLiteral("WinPE") : QStringLiteral("Windows"));
    emit initializationProgress(getVersionInfo());

    // Шаг 2: Найти соседний раздел данных
    logProgress(QStringLiteral("Scanning partitions..."));
    PartitionScanner scanner;
    std::optional<DriveInfo> dataDrive = scanner.findDataPartition();

    if (!dataDrive) {
        logProgress(QStringLiteral("No data partition found. Using local configuration only."));
        emit initializationProgress(QStringLiteral("Operating in standalone mode (no data partition)"));
    } else {
        logProgress(QStringLiteral("Data partition found: %1:\\").arg(dataDrive->letter));

        m_dataPartitionLetter = dataDrive->letter;
        m_dataPartitionInfo = std::make_unique<DriveInfo>(*dataDrive);

        // Шаг 3: Инициализировать DataLoader
        logProgress(QStringLiteral("Loading data from partition %1:\\...").arg(m_dataPartitionLetter));
        m_dataLoader = std::make_unique<DataLoader>(m_dataPartitionLetter);
        m_dataLoader->initialize();

        if (m_dataLoader->isInitialized()) {
            emit initializationProgress(QStringLiteral("Data partition loaded successfully"));

            // Шаг 4: Загрузить инструменты в ToolRegistry
            if (toolRegistry->loadFromDataPartition(m_dataPartitionLetter)) {
                logProgress(QStringLiteral("Tools loaded from data partition"));
            } else {
                logProgress(QStringLiteral("Failed to load tools from data partition"));
            }

            // Логирование загруженных ресурсов
            auto tools = m_dataLoader->getTools();
            auto drivers = m_dataLoader->getDrivers();
            auto images = m_dataLoader->getWindowsImages();

            emit initializationProgress(QStringLiteral("Loaded: %1 tools, %2 drivers, %3 Windows images")
                .arg(QString::number(tools.size()),
                     QString::number(drivers.size()),
                     QString::number(images.size())));

            if (auto wallpapers = m_dataLoader->getWallpapersDirectory()) {
                qInfo().noquote() << QStringLiteral("Wallpapers: %1").arg(*wallpapers);
            }

            if (auto config = m_dataLoader->getConfigDirectory()) {
                qInfo().noquote() << QStringLiteral("Config: %1").arg(*config);
            }

            if (auto resources = m_dataLoader->getResourcesDirectory()) {
                qInfo().noquote() << QStringLiteral("Resources: %1").arg(*resources);
            }
        } else {
            logError(QStringLiteral("Failed to initialize DataLoader"));
        }
    }

    // Шаг 5: Загрузить локальные инструменты
    logProgress(QStringLiteral("Loading local tools..."));
    toolRegistry->reload();

    auto allTools = toolRegistry->tools();
    emit initializationProgress(QStringLiteral("Total tools loaded: %1").arg(QString::number(allTools.size())));

    logProgress(QStringLiteral("Initialization complete!"));
    emit initializationComplete();

    return true;
}

bool ApplicationBootstrapper::isWinPEEnvironment()
{
    // Проверить, что мы в WinPE:
    // 1. Проверить наличие Windows\System32\boot\winpeshl.ini или подобное
    // 2. Проверить переменную окружения
    // 3. Проверить диск X: или других маркеров WinPE

    // Метод 1: Проверить переменную окружения SYSTEMROOT
    QByteArray systemRoot = qgetenv("SYSTEMROOT");
    if (systemRoot.startsWith("X:\\") || systemRoot.startsWith("Y:\\") || 
        systemRoot.startsWith("Z:\\") || systemRoot.startsWith("W:\\")) {
        return true;
    }

    // Метод 2: Проверить файлы WinPE
    if (QFileInfo::exists(QStringLiteral("X:\\Windows\\System32\\boot")) ||
        QFileInfo::exists(QStringLiteral("X:\\Windows\\System32\\winpeshl.ini"))) {
        return true;
    }

    // Метод 3: Проверить версию Windows (в WinPE обычно "Microsoft Windows (WinPE)")
    QString osName = QSysInfo::prettyProductName();
    if (osName.contains(QStringLiteral("PE"), Qt::CaseInsensitive)) {
        return true;
    }

    return false;
}

QString ApplicationBootstrapper::getVersionInfo()
{
    QString info = QStringLiteral("PhoenixShell v0.1.0");

    if (isWinPEEnvironment()) {
        info += QStringLiteral(" [WinPE]");
    } else {
        info += QStringLiteral(" [Windows]");
    }

    QString systemRoot = QString::fromUtf8(qgetenv("SYSTEMROOT"));
    if (!systemRoot.isEmpty()) {
        // Извлечь букву диска (например, "C" из "C:\Windows")
        if (systemRoot.length() > 0) {
            info += QStringLiteral(" - Drive: %1").arg(systemRoot[0]);
        }
    }

    return info;
}

void ApplicationBootstrapper::logProgress(const QString& message)
{
    qInfo().noquote() << message;
    emit initializationProgress(message);
}

void ApplicationBootstrapper::logError(const QString& errorMessage)
{
    qCritical().noquote() << errorMessage;
    emit initializationError(errorMessage);
}

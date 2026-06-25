#include "DataLoader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QDirIterator>

DataLoader::DataLoader(const QString& dataPartitionLetter)
    : m_dataPartitionLetter(dataPartitionLetter.toUpper())
{
}

void DataLoader::initialize()
{
    if (m_isInitialized) {
        return;
    }

    qInfo().noquote() << QStringLiteral("Initializing DataLoader for partition: %1:\\").arg(m_dataPartitionLetter);

    scanTools();
    scanDrivers();
    scanWindowsImages();
    scanPhoenixShellResources();

    m_isInitialized = true;

    qInfo().noquote() << QStringLiteral("DataLoader initialized: %1 tools, %2 drivers, %3 images")
        .arg(QString::number(m_tools.size()),
             QString::number(m_drivers.size()),
             QString::number(m_windowsImages.size()));
}

QVector<ToolData> DataLoader::getTools() const
{
    return m_tools;
}

QVector<ToolData> DataLoader::getToolsByCategory(const QString& category) const
{
    QVector<ToolData> result;
    for (const ToolData& tool : m_tools) {
        if (tool.category.compare(category, Qt::CaseInsensitive) == 0) {
            result.append(tool);
        }
    }
    return result;
}

QVector<DriverData> DataLoader::getDrivers() const
{
    return m_drivers;
}

QVector<DriverData> DataLoader::getDriversByCategory(const QString& category) const
{
    QVector<DriverData> result;
    for (const DriverData& driver : m_drivers) {
        if (driver.category.compare(category, Qt::CaseInsensitive) == 0) {
            result.append(driver);
        }
    }
    return result;
}

QVector<WindowsImageData> DataLoader::getWindowsImages() const
{
    return m_windowsImages;
}

std::optional<QString> DataLoader::getWallpapersDirectory() const
{
    if (m_wallpapersDir.isEmpty()) {
        return std::nullopt;
    }
    return m_wallpapersDir;
}

std::optional<QString> DataLoader::getConfigDirectory() const
{
    if (m_configDir.isEmpty()) {
        return std::nullopt;
    }
    return m_configDir;
}

std::optional<QString> DataLoader::getResourcesDirectory() const
{
    if (m_resourcesDir.isEmpty()) {
        return std::nullopt;
    }
    return m_resourcesDir;
}

void DataLoader::scanTools()
{
    QString toolsPath = buildPath(QStringLiteral("Tools"));
    QDir toolsDir(toolsPath);

    if (!toolsDir.exists()) {
        qWarning().noquote() << QStringLiteral("Tools directory not found: %1").arg(toolsPath);
        return;
    }

    QStringList toolDirs = toolsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    qInfo().noquote() << QStringLiteral("Found %1 tool directories").arg(QString::number(toolDirs.size()));

    for (const QString& toolDir : toolDirs) {
        QString fullPath = toolsPath + QStringLiteral("/") + toolDir;
        ToolData tool = parseToolDirectory(fullPath);
        if (tool.isValid) {
            m_tools.append(tool);
            qInfo().noquote() << QStringLiteral("  [+] Tool: %1 (%2)").arg(tool.name, tool.category);
        }
    }
}

void DataLoader::scanDrivers()
{
    QString driversPath = buildPath(QStringLiteral("Drivers"));
    QDir driversDir(driversPath);

    if (!driversDir.exists()) {
        qWarning().noquote() << QStringLiteral("Drivers directory not found: %1").arg(driversPath);
        return;
    }

    // Сканировать подпапки по категориям (ChipSet, GPU, Network, Audio, Storage)
    QStringList categories = { QStringLiteral("ChipSet"), QStringLiteral("GPU"), 
                               QStringLiteral("Network"), QStringLiteral("Audio"), 
                               QStringLiteral("Storage") };

    for (const QString& category : categories) {
        QString categoryPath = driversPath + QStringLiteral("/") + category;
        QDir categoryDir(categoryPath);

        if (!categoryDir.exists()) {
            continue;
        }

        QStringList driverFiles = categoryDir.entryList({ QStringLiteral("*.inf"), QStringLiteral("*.exe") });
        qInfo().noquote() << QStringLiteral("Found %1 drivers in category: %2")
            .arg(QString::number(driverFiles.size()), category);

        for (const QString& driverFile : driverFiles) {
            QString fullPath = categoryPath + QStringLiteral("/") + driverFile;
            DriverData driver = parseDriverFile(fullPath);
            driver.category = category;
            if (driver.isValid) {
                m_drivers.append(driver);
                qInfo().noquote() << QStringLiteral("  [+] Driver: %1 (%2)")
                    .arg(driver.name, category);
            }
        }
    }
}

void DataLoader::scanWindowsImages()
{
    QString imagesPath = buildPath(QStringLiteral("Windows"));
    QDir imagesDir(imagesPath);

    if (!imagesDir.exists()) {
        qWarning().noquote() << QStringLiteral("Windows images directory not found: %1").arg(imagesPath);
        return;
    }

    QStringList imageFiles = imagesDir.entryList({ QStringLiteral("*.wim"), QStringLiteral("*.esd"), 
                                                    QStringLiteral("*.iso") });
    qInfo().noquote() << QStringLiteral("Found %1 Windows images").arg(QString::number(imageFiles.size()));

    for (const QString& imageFile : imageFiles) {
        QString fullPath = imagesPath + QStringLiteral("/") + imageFile;
        WindowsImageData image = parseWindowsImage(fullPath);
        if (image.isValid) {
            m_windowsImages.append(image);
            qInfo().noquote() << QStringLiteral("  [+] Image: %1 (v%2, %3)")
                .arg(image.fileName, image.version, image.edition);
        }
    }
}

void DataLoader::scanPhoenixShellResources()
{
    QString phoenixPath = buildPath(QStringLiteral("PhoenixShell"));

    // Обои
    QString wallpapersPath = phoenixPath + QStringLiteral("/wallpapers");
    if (directoryExists(wallpapersPath)) {
        m_wallpapersDir = wallpapersPath;
        qInfo().noquote() << QStringLiteral("Wallpapers directory found: %1").arg(wallpapersPath);
    }

    // Конфигурация
    QString configPath = phoenixPath + QStringLiteral("/config");
    if (directoryExists(configPath)) {
        m_configDir = configPath;
        qInfo().noquote() << QStringLiteral("Config directory found: %1").arg(configPath);
    }

    // Ресурсы
    QString resourcesPath = phoenixPath + QStringLiteral("/resources");
    if (directoryExists(resourcesPath)) {
        m_resourcesDir = resourcesPath;
        qInfo().noquote() << QStringLiteral("Resources directory found: %1").arg(resourcesPath);
    }
}

ToolData DataLoader::parseToolDirectory(const QString& toolDir)
{
    ToolData tool;
    QDir dir(toolDir);

    if (!dir.exists()) {
        return tool;
    }

    tool.name = dir.dirName();
    tool.workingDir = toolDir;

    // Проверить tool.json первым
    QString jsonPath = toolDir + QStringLiteral("/tool.json");
    if (QFile::exists(jsonPath)) {
        QFile file(jsonPath);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();

            if (doc.isObject()) {
                tool = parseToolJson(doc.object());
                tool.workingDir = toolDir;
                return tool;
            }
        }
    }

    // Автоматический поиск exe файлов
    QStringList exeFiles = dir.entryList({ QStringLiteral("*.exe") });
    if (exeFiles.isEmpty()) {
        return tool; // Не валидный инструмент
    }

    tool.executable = toolDir + QStringLiteral("/") + exeFiles.first();
    tool.category = QStringLiteral("Tools");

    // Поиск иконки
    QStringList iconFiles = dir.entryList({ QStringLiteral("*.png"), QStringLiteral("*.ico"), 
                                             QStringLiteral("*.jpg") });
    if (!iconFiles.isEmpty()) {
        tool.iconPath = toolDir + QStringLiteral("/") + iconFiles.first();
    }

    tool.isValid = !tool.executable.isEmpty() && QFile::exists(tool.executable);
    return tool;
}

ToolData DataLoader::parseToolJson(const QJsonObject& json)
{
    ToolData tool;

    tool.name = json.value(QStringLiteral("name")).toString();
    tool.category = json.value(QStringLiteral("category")).toString(QStringLiteral("Tools"));
    tool.executable = json.value(QStringLiteral("executable")).toString();
    tool.iconPath = json.value(QStringLiteral("icon")).toString();
    tool.description = json.value(QStringLiteral("description")).toString();
    tool.arguments = json.value(QStringLiteral("arguments")).toString();
    tool.silentArguments = json.value(QStringLiteral("silentArguments"))
                               .toString(json.value(QStringLiteral("installArguments")).toString());

    tool.isValid = !tool.name.isEmpty() && !tool.executable.isEmpty();
    return tool;
}

DriverData DataLoader::parseDriverFile(const QString& driverPath)
{
    DriverData driver;
    QFileInfo fileInfo(driverPath);

    driver.name = fileInfo.baseName();
    driver.driverPath = driverPath;

    if (fileInfo.suffix().toLower() == QStringLiteral("inf")) {
        driver.infPath = driverPath;
    }

    driver.isValid = QFile::exists(driverPath);
    return driver;
}

WindowsImageData DataLoader::parseWindowsImage(const QString& imagePath)
{
    WindowsImageData image;
    QFileInfo fileInfo(imagePath);

    image.fileName = fileInfo.fileName();
    image.filePath = imagePath;
    image.fileSize = fileInfo.size();

    // Попытаться определить версию из имени файла
    if (image.fileName.contains(QStringLiteral("11"))) {
        image.version = QStringLiteral("11");
    } else if (image.fileName.contains(QStringLiteral("10"))) {
        image.version = QStringLiteral("10");
    } else {
        image.version = QStringLiteral("Unknown");
    }

    // Попытаться определить редакцию из имени файла
    if (image.fileName.contains(QStringLiteral("pro"), Qt::CaseInsensitive)) {
        image.edition = QStringLiteral("Pro");
    } else if (image.fileName.contains(QStringLiteral("enterprise"), Qt::CaseInsensitive)) {
        image.edition = QStringLiteral("Enterprise");
    } else if (image.fileName.contains(QStringLiteral("home"), Qt::CaseInsensitive)) {
        image.edition = QStringLiteral("Home");
    } else {
        image.edition = QStringLiteral("Unknown");
    }

    image.isValid = QFile::exists(imagePath) && image.fileSize > 0;
    return image;
}

QString DataLoader::buildPath(const QString& subPath) const
{
    return m_dataPartitionLetter + QStringLiteral(":\\") + subPath;
}

bool DataLoader::directoryExists(const QString& path) const
{
    return QDir(path).exists();
}

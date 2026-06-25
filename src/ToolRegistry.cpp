#include "ToolRegistry.h"
#include "DataLoader.h"

#include "PathUtils.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool ToolEntry::isValid() const
{
    return !name.trimmed().isEmpty() && !path.trimmed().isEmpty();
}

ToolRegistry::ToolRegistry(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<ToolEntry>("ToolEntry");
}

ToolRegistry::~ToolRegistry() = default;

void ToolRegistry::reload()
{
    QVector<ToolEntry> merged;

    for (const ToolEntry& entry : readConfiguredTools()) {
        addUnique(merged, entry);
    }

    for (const ToolEntry& entry : scanToolDirectories()) {
        addUnique(merged, entry);
    }

    m_tools = merged;
    qInfo().noquote() << QStringLiteral("ToolRegistry loaded %1 tools").arg(QString::number(m_tools.size()));
    emit reloaded();
}

QVector<ToolEntry> ToolRegistry::tools() const
{
    return m_tools;
}

QVector<ToolEntry> ToolRegistry::toolsForCategory(const QString& category) const
{
    QVector<ToolEntry> result;
    for (const ToolEntry& tool : m_tools) {
        if (tool.category.compare(category, Qt::CaseInsensitive) == 0) {
            result.push_back(tool);
        }
    }
    return result;
}

QVector<ToolEntry> ToolRegistry::readConfiguredTools() const
{
    QVector<ToolEntry> result;

    QFile file(PathUtils::configPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning().noquote() << QStringLiteral("ToolRegistry config not found: %1").arg(PathUtils::configPath());
        return result;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning().noquote() << QStringLiteral("ToolRegistry config parse error: %1").arg(error.errorString());
        return result;
    }
    const QJsonArray tools = document.object().value(QStringLiteral("tools")).toArray();
    for (const QJsonValue& value : tools) {
        const QJsonObject object = value.toObject();
        ToolEntry entry;
        entry.name = object.value(QStringLiteral("name")).toString().trimmed();
        entry.path = PathUtils::resolveLocalPath(object.value(QStringLiteral("path")).toString());
        entry.iconPath = PathUtils::resolveLocalPath(object.value(QStringLiteral("icon")).toString());
        entry.category = object.value(QStringLiteral("category")).toString(QStringLiteral("Tools")).trimmed();
        entry.workingDirectory = QFileInfo(entry.path).absolutePath();

        if (!entry.isValid()) {
            continue;
        }

        if (!PathUtils::isAllowedExecutable(entry.path)) {
            qWarning().noquote() << QStringLiteral("Skipping unavailable or non-local tool: %1 (%2)")
                                    .arg(entry.name, entry.path);
            continue;
        }

        result.push_back(entry);
    }

    return result;
}

QVector<ToolEntry> ToolRegistry::scanToolDirectories() const
{
    QVector<ToolEntry> result;
    const QDir toolsRoot(QDir(PathUtils::dataRoot()).filePath(QStringLiteral("Tools")));
    if (!toolsRoot.exists()) {
        qInfo().noquote() << QStringLiteral("Tools directory not found: %1").arg(toolsRoot.absolutePath());
        return result;
    }

    const QFileInfoList directories = toolsRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& directory : directories) {
        QDir toolDir(directory.absoluteFilePath());
        const QString preferred = toolDir.filePath(directory.baseName() + QStringLiteral(".exe"));
        QString executable;
        if (QFileInfo::exists(preferred)) {
            executable = preferred;
        } else {
            const QFileInfoList executables = toolDir.entryInfoList(QStringList() << QStringLiteral("*.exe"),
                                                                    QDir::Files,
                                                                    QDir::Name);
            if (!executables.isEmpty()) {
                executable = executables.first().absoluteFilePath();
            }
        }

        if (executable.isEmpty() || !PathUtils::isAllowedExecutable(executable)) {
            continue;
        }

        ToolEntry entry;
        entry.name = directory.baseName();
        entry.path = PathUtils::resolveLocalPath(executable);
        entry.iconPath = PathUtils::resolveLocalPath(toolDir.filePath(QStringLiteral("icon.png")));
        entry.category = QStringLiteral("Tools");
        entry.workingDirectory = QFileInfo(entry.path).absolutePath();
        result.push_back(entry);
    }

    return result;
}

void ToolRegistry::addUnique(QVector<ToolEntry>& list, const ToolEntry& entry) const
{
    const QString path = QFileInfo(entry.path).canonicalFilePath().toCaseFolded();
    for (const ToolEntry& existing : list) {
        if (QFileInfo(existing.path).canonicalFilePath().toCaseFolded() == path
            || existing.name.compare(entry.name, Qt::CaseInsensitive) == 0) {
            return;
        }
    }

    list.push_back(entry);
}

bool ToolRegistry::loadFromDataPartition(const QString& dataPartitionLetter)
{
    if (dataPartitionLetter.isEmpty()) {
        qWarning().noquote() << QStringLiteral("Invalid data partition letter");
        return false;
    }

    m_dataPartitionLetter = dataPartitionLetter.toUpper();
    m_dataLoader = std::make_unique<DataLoader>(m_dataPartitionLetter);

    if (!m_dataLoader) {
        qWarning().noquote() << QStringLiteral("Failed to create DataLoader");
        return false;
    }

    m_dataLoader->initialize();

    if (!m_dataLoader->isInitialized()) {
        qWarning().noquote() << QStringLiteral("DataLoader failed to initialize");
        m_dataLoader.reset();
        return false;
    }

    // Загрузить инструменты из соседнего раздела
    QVector<ToolEntry> dataTools = loadFromDataLoader(m_dataLoader.get());
    for (const ToolEntry& entry : dataTools) {
        addUnique(m_tools, entry);
    }

    qInfo().noquote() << QStringLiteral("Loaded %1 tools from data partition %2:\\")
        .arg(QString::number(dataTools.size()), m_dataPartitionLetter);

    emit dataPartitionLoaded(m_dataPartitionLetter);
    return true;
}

QVector<ToolEntry> ToolRegistry::loadFromDataLoader(DataLoader* loader) const
{
    QVector<ToolEntry> result;

    if (!loader || !loader->isInitialized()) {
        return result;
    }

    // Получить все инструменты из DataLoader
    QVector<ToolData> allTools = loader->getTools();

    for (const ToolData& tool : allTools) {
        if (!tool.isValid) {
            continue;
        }

        ToolEntry entry;
        entry.name = tool.name;
        entry.path = tool.executable;
        entry.iconPath = tool.iconPath;
        entry.category = tool.category;
        entry.workingDirectory = tool.workingDir;

        if (!entry.isValid()) {
            qWarning().noquote() << QStringLiteral("Invalid tool entry: %1").arg(tool.name);
            continue;
        }

        // Проверить, что файл существует
        if (!QFile::exists(entry.path)) {
            qWarning().noquote() << QStringLiteral("Tool executable not found: %1").arg(entry.path);
            continue;
        }

        result.push_back(entry);
    }

    qInfo().noquote() << QStringLiteral("Loaded %1 tools from DataLoader").arg(QString::number(result.size()));
    return result;
}

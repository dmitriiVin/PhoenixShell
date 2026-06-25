#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <memory>

struct ToolEntry {
    QString name;
    QString path;
    QString iconPath;
    QString category;
    QString workingDirectory;

    bool isValid() const;
};

Q_DECLARE_METATYPE(ToolEntry)

class DataLoader;

/**
 * @brief Реестр всех доступных инструментов
 * 
 * Загружает инструменты из:
 * 1. Конфигурации tools.json (локально)
 * 2. Папки Tools/ (локально)
 * 3. Соседнего раздела флешки (если доступен)
 */
class ToolRegistry final : public QObject {
    Q_OBJECT

public:
    explicit ToolRegistry(QObject* parent = nullptr);
    ~ToolRegistry();

    void reload();
    QVector<ToolEntry> tools() const;
    QVector<ToolEntry> toolsForCategory(const QString& category) const;

    /**
     * @brief Загрузить инструменты с соседнего раздела данных
     * @param dataPartitionLetter Буква раздела (например, "D")
     * @return true если успешно загружено, false если раздел недоступен
     */
    bool loadFromDataPartition(const QString& dataPartitionLetter);

    /**
     * @brief Получить букву раздела данных (если используется)
     */
    QString getDataPartitionLetter() const { return m_dataPartitionLetter; }

signals:
    void reloaded();
    void dataPartitionLoaded(const QString& letter);

private:
    QVector<ToolEntry> readConfiguredTools() const;
    QVector<ToolEntry> scanToolDirectories() const;
    QVector<ToolEntry> loadFromDataLoader(DataLoader* loader) const;
    void addUnique(QVector<ToolEntry>& list, const ToolEntry& entry) const;

    QVector<ToolEntry> m_tools;
    QString m_dataPartitionLetter;
    std::unique_ptr<DataLoader> m_dataLoader;
};

#pragma once

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <optional>
#include <filesystem>

/**
 * @brief Контейнер для информации об инструменте из раздела данных
 */
struct ToolData {
    QString name;
    QString category;
    QString executable;
    QString iconPath;
    QString workingDir;
    QString description;
    QString arguments;
    QString silentArguments;
    bool isValid = false;
};

/**
 * @brief Информация о драйвере
 */
struct DriverData {
    QString name;
    QString category;     // "ChipSet", "GPU", "Network", "Audio", "Storage"
    QString driverPath;
    QString infPath;
    QString description;
    bool isValid = false;
};

/**
 * @brief Информация об образе Windows
 */
struct WindowsImageData {
    QString fileName;
    QString filePath;
    QString edition;      // "Home", "Pro", "Enterprise"
    QString version;      // "10", "11"
    quint64 fileSize = 0;
    bool isValid = false;
};

/**
 * @brief Загрузчик данных с соседнего раздела (программы, драйверы, образы)
 * 
 * Сканирует структуру:
 * ```
 * D:\ (соседний раздел)
 * ├── Tools/
 * │   ├── AIDA64/
 * │   │   ├── aida64.exe
 * │   │   ├── icon.png
 * │   │   └── tool.json (опционально)
 * │   └── ...
 * ├── Drivers/
 * │   ├── ChipSet/
 * │   ├── GPU/
 * │   ├── Network/
 * │   └── Audio/
 * ├── Windows/
 * │   ├── win11.wim
 * │   ├── win10.wim
 * │   └── ...
 * └── PhoenixShell/
 *     ├── wallpapers/
 *     ├── config/
 *     └── resources/
 * ```
 */
class DataLoader final {
public:
    explicit DataLoader(const QString& dataPartitionLetter);

    /**
     * @brief Инициализировать загрузчик (сканировать все данные)
     */
    void initialize();

    /**
     * @brief Получить все найденные инструменты
     */
    QVector<ToolData> getTools() const;

    /**
     * @brief Получить инструменты по категории
     */
    QVector<ToolData> getToolsByCategory(const QString& category) const;

    /**
     * @brief Получить все найденные драйверы
     */
    QVector<DriverData> getDrivers() const;

    /**
     * @brief Получить драйверы по категории
     */
    QVector<DriverData> getDriversByCategory(const QString& category) const;

    /**
     * @brief Получить все найденные образы Windows
     */
    QVector<WindowsImageData> getWindowsImages() const;

    /**
     * @brief Получить путь к папке обоев, если существует
     */
    std::optional<QString> getWallpapersDirectory() const;

    /**
     * @brief Получить путь к конфигурации, если существует
     */
    std::optional<QString> getConfigDirectory() const;

    /**
     * @brief Получить путь к ресурсам, если существует
     */
    std::optional<QString> getResourcesDirectory() const;

    /**
     * @brief Проверить, доступен ли раздел данных
     */
    bool isInitialized() const { return m_isInitialized; }

    /**
     * @brief Получить букву раздела данных
     */
    QString getDataPartitionLetter() const { return m_dataPartitionLetter; }

private:
    QString m_dataPartitionLetter;
    bool m_isInitialized = false;

    QVector<ToolData> m_tools;
    QVector<DriverData> m_drivers;
    QVector<WindowsImageData> m_windowsImages;

    QString m_wallpapersDir;
    QString m_configDir;
    QString m_resourcesDir;

    // Методы сканирования
    void scanTools();
    void scanDrivers();
    void scanWindowsImages();
    void scanPhoenixShellResources();

    // Вспомогательные методы
    ToolData parseToolDirectory(const QString& toolDir);
    ToolData parseToolJson(const QJsonObject& json);
    DriverData parseDriverFile(const QString& driverPath);
    WindowsImageData parseWindowsImage(const QString& imagePath);

    QString buildPath(const QString& subPath) const;
    bool directoryExists(const QString& path) const;
};

#pragma once

#include <QString>
#include <QVector>
#include <QFileInfo>
#include <optional>
#include <cstdint>

/**
 * @brief Информация о разделе/диске
 */
struct DriveInfo {
    QString letter;          // "C:", "D:", "X:"
    QString label;           // Метка диска
    quint64 totalSize = 0;   // Байты
    quint64 freeSize = 0;    // Байты
    QString fileSystem;      // "NTFS", "FAT32", "UDF"
    bool isRemovable = false; // Флешка/USB?
    bool isValid = false;
};

/**
 * @brief Сканер разделов флешки для поиска программ, драйверов и образов
 * 
 * Предназначен для работы с двухраздельными флешками:
 * - Раздел 1 (WinPE): запускаемая система с PhoenixShell
 * - Раздел 2 (NTFS): программы, драйверы, образы Windows
 */
class PartitionScanner final {
public:
    PartitionScanner();

    /**
     * @brief Сканирует все доступные диски и находит соседний раздел
     * @return Информация о найденном разделе, или пусто если не найден
     */
    std::optional<DriveInfo> findDataPartition();

    /**
     * @brief Получить список всех доступных дисков
     */
    QVector<DriveInfo> getAllDrives() const;

    /**
     * @brief Проверить, является ли диск съёмным
     */
    static bool isRemovableDrive(const QString& driveLetter);

    /**
     * @brief Получить информацию о диске
     */
    static std::optional<DriveInfo> getDriveInfo(const QString& driveLetter);

    /**
     * @brief Найти букву диска WinPE (обычно X: в WinPE среде)
     */
    static std::optional<QString> findWinPEDrive();

    /**
     * @brief Получить соседний раздел на флешке
     * @param currentDrive Текущий диск (например, X:)
     * @return Соседний раздел (например, Y:), или пусто
     */
    static std::optional<QString> findAdjacentPartition(const QString& currentDrive);

private:
    QVector<DriveInfo> scanDrives() const;
    static bool isValidWindowsDrive(const QString& driveLetter);
};

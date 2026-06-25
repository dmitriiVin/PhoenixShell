#include "PartitionScanner.h"

#include <QDebug>
#include <QDir>
#include <QStorageInfo>
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "ole32.lib")

PartitionScanner::PartitionScanner() = default;

std::optional<DriveInfo> PartitionScanner::findDataPartition()
{
    // 1. Найти текущий диск WinPE
    auto winpeDrive = findWinPEDrive();
    if (!winpeDrive) {
        qWarning().noquote() << QStringLiteral("Could not determine WinPE drive letter");
        return std::nullopt;
    }

    qInfo().noquote() << QStringLiteral("WinPE drive: %1").arg(*winpeDrive);

    // 2. Найти соседний раздел
    auto dataDrive = findAdjacentPartition(*winpeDrive);
    if (!dataDrive) {
        qWarning().noquote() << QStringLiteral("No adjacent data partition found");
        return std::nullopt;
    }

    qInfo().noquote() << QStringLiteral("Data partition: %1").arg(*dataDrive);

    // 3. Получить информацию о разделе
    auto driveInfo = getDriveInfo(*dataDrive);
    if (!driveInfo || !driveInfo->isValid) {
        qWarning().noquote() << QStringLiteral("Data partition is not accessible: %1").arg(*dataDrive);
        return std::nullopt;
    }

    return driveInfo;
}

QVector<DriveInfo> PartitionScanner::getAllDrives() const
{
    return scanDrives();
}

bool PartitionScanner::isRemovableDrive(const QString& driveLetter)
{
    QString drivePath = driveLetter + QStringLiteral("\\");
    
    UINT driveType = GetDriveTypeW(reinterpret_cast<LPCWSTR>(drivePath.utf16()));
    
    return driveType == DRIVE_REMOVABLE || driveType == DRIVE_FIXED;
}

std::optional<DriveInfo> PartitionScanner::getDriveInfo(const QString& driveLetter)
{
    if (driveLetter.length() < 1) {
        return std::nullopt;
    }

    // Проверить, что диск доступен
    QString drivePath = driveLetter + QStringLiteral(":\\");
    if (!isValidWindowsDrive(driveLetter)) {
        qWarning().noquote() << QStringLiteral("Drive not accessible: %1").arg(driveLetter);
        return std::nullopt;
    }

    DriveInfo info;
    info.letter = driveLetter;
    info.isValid = true;

    // Получить метку и файловую систему
    wchar_t volumeName[MAX_PATH + 1];
    wchar_t fileSystemName[MAX_PATH + 1];
    DWORD serialNumber = 0;
    DWORD maxComponentLength = 0;
    DWORD fileSystemFlags = 0;

    bool success = GetVolumeInformationW(
        reinterpret_cast<LPCWSTR>(drivePath.utf16()),
        volumeName, sizeof(volumeName),
        &serialNumber,
        &maxComponentLength,
        &fileSystemFlags,
        fileSystemName, sizeof(fileSystemName)
    );

    if (success) {
        info.label = QString::fromWCharArray(volumeName);
        info.fileSystem = QString::fromWCharArray(fileSystemName);
    }

    // Получить размеры
    ULARGE_INTEGER freeBytes, totalBytes;
    if (GetDiskFreeSpaceExW(
            reinterpret_cast<LPCWSTR>(drivePath.utf16()),
            &freeBytes, &totalBytes, nullptr)) {
        info.totalSize = totalBytes.QuadPart;
        info.freeSize = freeBytes.QuadPart;
    }

    info.isRemovable = isRemovableDrive(driveLetter);

    qInfo().noquote() << QStringLiteral("Drive %1: %2 (%3) - %4 / %5 bytes, FS=%6")
        .arg(driveLetter, info.label, info.isRemovable ? QStringLiteral("USB") : QStringLiteral("Local"),
             QString::number(info.freeSize), QString::number(info.totalSize), info.fileSystem);

    return info;
}

std::optional<QString> PartitionScanner::findWinPEDrive()
{
    // В WinPE окружении, обычно система запускается с диска X:
    // Проверим стандартные буквы
    QStringList potentialDrives = { QStringLiteral("X"), QStringLiteral("Y"), 
                                      QStringLiteral("Z"), QStringLiteral("W"),
                                      QStringLiteral("V") };

    for (const QString& drive : potentialDrives) {
        if (isValidWindowsDrive(drive)) {
            // Проверить, есть ли признаки WinPE (например, наличие System32\boot)
            QString bootPath = drive + QStringLiteral(":\\Windows\\System32\\boot");
            QDir dir(bootPath);
            if (dir.exists()) {
                qInfo().noquote() << QStringLiteral("Found WinPE drive: %1").arg(drive);
                return drive;
            }
        }
    }

    // Если стандартные не найдены, ищем любой доступный диск
    for (const DriveInfo& drive : getAllDrives()) {
        if (drive.isValid && drive.letter != QStringLiteral("C")) {
            return drive.letter;
        }
    }

    return std::nullopt;
}

std::optional<QString> PartitionScanner::findAdjacentPartition(const QString& currentDrive)
{
    if (currentDrive.isEmpty()) {
        return std::nullopt;
    }

    // Получить все диски
    QVector<DriveInfo> allDrives;
    for (QChar letter : QStringLiteral("XYZVWUTSR")) {
        auto info = getDriveInfo(letter);
        if (info) {
            allDrives.append(*info);
        }
    }

    if (allDrives.size() < 2) {
        qWarning().noquote() << QStringLiteral("Not enough partitions for dual-partition setup");
        return std::nullopt;
    }

    // Найти текущий диск в списке
    int currentIndex = -1;
    for (int i = 0; i < allDrives.size(); ++i) {
        if (allDrives[i].letter.compare(currentDrive, Qt::CaseInsensitive) == 0) {
            currentIndex = i;
            break;
        }
    }

    if (currentIndex == -1) {
        qWarning().noquote() << QStringLiteral("Current drive %1 not found in list").arg(currentDrive);
        return std::nullopt;
    }

    // Вернуть соседний раздел на той же флешке
    // Приоритет: NTFS для данных, затем другие форматы
    if (currentIndex + 1 < allDrives.size()) {
        const DriveInfo& nextDrive = allDrives[currentIndex + 1];
        if (nextDrive.fileSystem == QStringLiteral("NTFS") ||
            nextDrive.fileSystem == QStringLiteral("FAT32")) {
            qInfo().noquote() << QStringLiteral("Found adjacent partition: %1 (%2)")
                .arg(nextDrive.letter, nextDrive.fileSystem);
            return nextDrive.letter;
        }
    }

    if (currentIndex - 1 >= 0) {
        const DriveInfo& prevDrive = allDrives[currentIndex - 1];
        if (prevDrive.fileSystem == QStringLiteral("NTFS") ||
            prevDrive.fileSystem == QStringLiteral("FAT32")) {
            qInfo().noquote() << QStringLiteral("Found adjacent partition: %1 (%2)")
                .arg(prevDrive.letter, prevDrive.fileSystem);
            return prevDrive.letter;
        }
    }

    return std::nullopt;
}

QVector<DriveInfo> PartitionScanner::scanDrives() const
{
    QVector<DriveInfo> drives;

    for (QChar letter : QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ")) {
        auto info = getDriveInfo(letter);
        if (info) {
            drives.append(*info);
        }
    }

    return drives;
}

bool PartitionScanner::isValidWindowsDrive(const QString& driveLetter)
{
    if (driveLetter.isEmpty()) {
        return false;
    }

    QString path = driveLetter + QStringLiteral(":\\");
    return QDir(path).exists();
}

#include "DiagnosticsModule.h"

#include <QDateTime>
#include <QDir>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QPushButton>
#include <QStorageInfo>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <windows.h>

namespace {

QString formatBytes(quint64 value)
{
    const QStringList units = {QStringLiteral("B"), QStringLiteral("KB"), QStringLiteral("MB"), QStringLiteral("GB"), QStringLiteral("TB")};
    double size = static_cast<double>(value);
    int unit = 0;
    while (size >= 1024.0 && unit < units.size() - 1) {
        size /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2").arg(size, 0, 'f', unit == 0 ? 0 : 1).arg(units.at(unit));
}

QString registryString(HKEY root, const wchar_t* path, const wchar_t* valueName)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, path, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return {};
    }

    wchar_t buffer[512] = {};
    DWORD size = sizeof(buffer);
    DWORD type = REG_SZ;
    QString result;
    if (RegQueryValueExW(key, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(buffer), &size) == ERROR_SUCCESS
        && type == REG_SZ) {
        result = QString::fromWCharArray(buffer);
    }
    RegCloseKey(key);
    return result.trimmed();
}

} // namespace

DiagnosticsModule::DiagnosticsModule(QWidget* parent)
    : QWidget(parent)
    , m_output(new QTextBrowser(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);

    auto* refreshButton = new QPushButton(QStringLiteral("Refresh"), this);
    layout->addWidget(refreshButton, 0, Qt::AlignLeft);
    layout->addWidget(m_output, 1);

    connect(refreshButton, &QPushButton::clicked, this, &DiagnosticsModule::refresh);
    refresh();
}

void DiagnosticsModule::refresh()
{
    m_output->setPlainText(QStringLiteral("PhoenixShell Diagnostics\n%1\n\n%2\n%3\n%4\n%5\n")
                               .arg(QDateTime::currentDateTime().toString(Qt::ISODate),
                                    collectSystemInfo(),
                                    collectGpuInfo(),
                                    collectDiskInfo(),
                                    collectNetworkInfo())
                           + collectSmartInfo());
}

QString DiagnosticsModule::collectSystemInfo() const
{
    SYSTEM_INFO systemInfo{};
    GetNativeSystemInfo(&systemInfo);

    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    GlobalMemoryStatusEx(&memory);

    const QString cpuName = registryString(HKEY_LOCAL_MACHINE,
                                           L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                                           L"ProcessorNameString");

    QString text;
    text += QStringLiteral("CPU\n");
    text += QStringLiteral("  Name: %1\n").arg(cpuName.isEmpty() ? QStringLiteral("Unknown") : cpuName);
    text += QStringLiteral("  Logical processors: %1\n").arg(systemInfo.dwNumberOfProcessors);
    text += QStringLiteral("  Page size: %1\n\n").arg(systemInfo.dwPageSize);
    text += QStringLiteral("RAM\n");
    text += QStringLiteral("  Total: %1\n").arg(formatBytes(memory.ullTotalPhys));
    text += QStringLiteral("  Available: %1\n").arg(formatBytes(memory.ullAvailPhys));
    text += QStringLiteral("  Load: %1%\n\n").arg(memory.dwMemoryLoad);
    return text;
}

QString DiagnosticsModule::collectDiskInfo() const
{
    QString text = QStringLiteral("Disks / Volumes\n");
    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo& volume : volumes) {
        if (!volume.isValid() || !volume.isReady()) {
            continue;
        }

        text += QStringLiteral("  %1  %2 free / %3 total  fs=%4\n")
                    .arg(QDir::toNativeSeparators(volume.rootPath()),
                         formatBytes(volume.bytesAvailable()),
                         formatBytes(volume.bytesTotal()),
                         QString::fromLatin1(volume.fileSystemType()));
    }
    text += QLatin1Char('\n');
    return text;
}

QString DiagnosticsModule::collectNetworkInfo() const
{
    QString text = QStringLiteral("Network\n");
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (!iface.flags().testFlag(QNetworkInterface::IsUp)
            || iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }

        text += QStringLiteral("  %1  mac=%2\n").arg(iface.humanReadableName(), iface.hardwareAddress());
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            text += QStringLiteral("    IP: %1  mask: %2  gateway/DNS: query via ipconfig\n")
                        .arg(entry.ip().toString(), entry.netmask().toString());
        }
    }
    text += QLatin1Char('\n');
    return text;
}

QString DiagnosticsModule::collectGpuInfo() const
{
    QString text = QStringLiteral("GPU\n");
    DISPLAY_DEVICEW device{};
    device.cb = sizeof(device);

    for (DWORD index = 0; EnumDisplayDevicesW(nullptr, index, &device, 0); ++index) {
        text += QStringLiteral("  %1  %2\n")
                    .arg(QString::fromWCharArray(device.DeviceString),
                         QString::fromWCharArray(device.DeviceName));
        ZeroMemory(&device, sizeof(device));
        device.cb = sizeof(device);
    }

    if (text == QStringLiteral("GPU\n")) {
        text += QStringLiteral("  No display adapter reported by EnumDisplayDevices.\n");
    }
    text += QLatin1Char('\n');
    return text;
}

QString DiagnosticsModule::collectSmartInfo() const
{
    QString text = QStringLiteral("SMART\n");
    bool anyDrive = false;

    for (int i = 0; i < 32; ++i) {
        const QString devicePath = QStringLiteral("\\\\.\\PhysicalDrive%1").arg(i);
        const std::wstring path = devicePath.toStdWString();
        HANDLE handle = CreateFileW(path.c_str(),
                                    0,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr,
                                    OPEN_EXISTING,
                                    0,
                                    nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            continue;
        }

        anyDrive = true;
        text += QStringLiteral("  PhysicalDrive%1: accessible for SMART/vendor diagnostic tools\n").arg(i);
        CloseHandle(handle);
    }

    if (!anyDrive) {
        text += QStringLiteral("  No physical drives opened. Storage drivers may be missing in WinPE.\n");
    }

    return text;
}

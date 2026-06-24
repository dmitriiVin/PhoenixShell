#include "ApplicationLauncher.h"

#include "PathUtils.h"

#include <QDir>
#include <QDebug>
#include <QFileInfo>

#include <windows.h>

LaunchResult ApplicationLauncher::launchDetached(const ToolEntry& tool)
{
    LaunchResult result;
    const QString executable = PathUtils::resolveLocalPath(tool.path);
    if (!PathUtils::isAllowedExecutable(executable)) {
        result.error = QStringLiteral("Executable is missing or outside PhoenixShell local roots: %1").arg(executable);
        return result;
    }

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInfo{};
    std::wstring application = QDir::toNativeSeparators(executable).toStdWString();
    std::wstring commandLine = L"\"" + application + L"\"";
    std::wstring workingDirectory = QDir::toNativeSeparators(QFileInfo(executable).absolutePath()).toStdWString();

    const BOOL created = CreateProcessW(application.c_str(),
                                        commandLine.data(),
                                        nullptr,
                                        nullptr,
                                        FALSE,
                                        CREATE_NEW_PROCESS_GROUP,
                                        nullptr,
                                        workingDirectory.c_str(),
                                        &startupInfo,
                                        &processInfo);

    if (!created) {
        result.error = win32ErrorMessage(GetLastError());
        qWarning().noquote() << QStringLiteral("CreateProcessW failed for %1: %2").arg(executable, result.error);
        return result;
    }

    result.ok = true;
    result.processId = processInfo.dwProcessId;
    qInfo().noquote() << QStringLiteral("Started external process %1 pid=%2").arg(tool.name).arg(result.processId);

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return result;
}

QString ApplicationLauncher::win32ErrorMessage(unsigned long errorCode)
{
    LPWSTR buffer = nullptr;
    const DWORD size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER
                                          | FORMAT_MESSAGE_FROM_SYSTEM
                                          | FORMAT_MESSAGE_IGNORE_INSERTS,
                                      nullptr,
                                      errorCode,
                                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                      reinterpret_cast<LPWSTR>(&buffer),
                                      0,
                                      nullptr);

    QString message = size > 0 && buffer
        ? QString::fromWCharArray(buffer, static_cast<int>(size)).trimmed()
        : QStringLiteral("Win32 error %1").arg(errorCode);

    if (buffer) {
        LocalFree(buffer);
    }

    return message;
}

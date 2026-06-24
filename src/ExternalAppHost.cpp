#include "ExternalAppHost.h"

#include "ApplicationLauncher.h"
#include "PathUtils.h"

#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QLabel>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWindow>

namespace {

struct EnumWindowContext {
    DWORD processId = 0;
    HWND window = nullptr;
};

} // namespace

ExternalAppHost::ExternalAppHost(const ToolEntry& tool, QWidget* parent)
    : QWidget(parent)
    , m_tool(tool)
    , m_windowPollTimer(new QTimer(this))
    , m_processPollTimer(new QTimer(this))
    , m_layout(new QVBoxLayout(this))
    , m_statusLabel(new QLabel(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_layout->addWidget(m_statusLabel);

    connect(m_windowPollTimer, &QTimer::timeout, this, &ExternalAppHost::pollForWindow);
    connect(m_processPollTimer, &QTimer::timeout, this, &ExternalAppHost::pollProcessExit);

    showStatus(QStringLiteral("Starting %1...").arg(m_tool.name));
}

ExternalAppHost::~ExternalAppHost()
{
    if (m_embeddedHwnd && IsWindow(m_embeddedHwnd)) {
        PostMessageW(m_embeddedHwnd, WM_CLOSE, 0, 0);
    }

    if (m_processInfo.hThread) {
        CloseHandle(m_processInfo.hThread);
    }
    if (m_processInfo.hProcess) {
        CloseHandle(m_processInfo.hProcess);
    }
}

void ExternalAppHost::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_embeddedHwnd && IsWindow(m_embeddedHwnd)) {
        SetWindowPos(m_embeddedHwnd,
                     nullptr,
                     0,
                     0,
                     event->size().width(),
                     event->size().height(),
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void ExternalAppHost::start()
{
    if (m_started) {
        return;
    }
    m_started = true;

    const QString executable = PathUtils::resolveLocalPath(m_tool.path);
    if (!PathUtils::isAllowedExecutable(executable)) {
        const QString message = QStringLiteral("Executable is missing or blocked by local-only policy: %1").arg(executable);
        showStatus(message);
        emit failed(message);
        return;
    }

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_SHOWNORMAL;

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
                                        &m_processInfo);

    if (!created) {
        const QString message = ApplicationLauncher::win32ErrorMessage(GetLastError());
        showStatus(QStringLiteral("Failed to start %1: %2").arg(m_tool.name, message));
        emit failed(message);
        return;
    }

    qInfo().noquote() << QStringLiteral("Started hosted process %1 pid=%2")
                         .arg(m_tool.name)
                         .arg(m_processInfo.dwProcessId);
    emit processStarted(m_processInfo.dwProcessId);

    m_windowPollTimer->start(250);
    m_processPollTimer->start(1000);
}

void ExternalAppHost::pollForWindow()
{
    if (m_embeddedHwnd) {
        m_windowPollTimer->stop();
        return;
    }

    HWND hwnd = findProcessWindow();
    if (hwnd) {
        embedWindow(hwnd);
        m_windowPollTimer->stop();
        return;
    }

    ++m_pollAttempts;
    if (m_pollAttempts > 80) {
        m_windowPollTimer->stop();
        showStatus(QStringLiteral("%1 is running, but no visible main window was found.").arg(m_tool.name));
        qWarning().noquote() << QStringLiteral("No visible window found for hosted process pid=%1")
                                .arg(m_processInfo.dwProcessId);
    }
}

void ExternalAppHost::pollProcessExit()
{
    if (!m_processInfo.hProcess) {
        return;
    }

    DWORD exitCode = STILL_ACTIVE;
    if (!GetExitCodeProcess(m_processInfo.hProcess, &exitCode)) {
        return;
    }

    if (exitCode != STILL_ACTIVE) {
        m_processPollTimer->stop();
        m_windowPollTimer->stop();
        showStatus(QStringLiteral("%1 exited with code %2.").arg(m_tool.name).arg(exitCode));
        qInfo().noquote() << QStringLiteral("Hosted process exited pid=%1 code=%2")
                             .arg(m_processInfo.dwProcessId)
                             .arg(exitCode);
        emit processExited(m_processInfo.dwProcessId, exitCode);
    }
}

BOOL CALLBACK ExternalAppHost::enumWindowsProc(HWND hwnd, LPARAM lParam)
{
    auto* context = reinterpret_cast<EnumWindowContext*>(lParam);
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);

    if (processId != context->processId) {
        return TRUE;
    }

    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != nullptr) {
        return TRUE;
    }

    context->window = hwnd;
    return FALSE;
}

HWND ExternalAppHost::findProcessWindow() const
{
    EnumWindowContext context;
    context.processId = m_processInfo.dwProcessId;
    EnumWindows(&ExternalAppHost::enumWindowsProc, reinterpret_cast<LPARAM>(&context));
    return context.window;
}

void ExternalAppHost::embedWindow(HWND hwnd)
{
    m_embeddedHwnd = hwnd;

    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_POPUP);
    style |= WS_CHILD | WS_VISIBLE;
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);
    SetParent(hwnd, reinterpret_cast<HWND>(winId()));

    const WId windowId = reinterpret_cast<WId>(hwnd);
    m_foreignWindow = QWindow::fromWinId(windowId);
    m_windowContainer = QWidget::createWindowContainer(m_foreignWindow, this);
    m_windowContainer->setFocusPolicy(Qt::StrongFocus);

    m_layout->removeWidget(m_statusLabel);
    m_statusLabel->hide();
    m_layout->addWidget(m_windowContainer);

    ShowWindow(hwnd, SW_SHOW);
    SetWindowPos(hwnd, nullptr, 0, 0, width(), height(), SWP_NOZORDER | SWP_FRAMECHANGED);
    qInfo().noquote() << QStringLiteral("Embedded process window hwnd=%1 pid=%2")
                         .arg(reinterpret_cast<quintptr>(hwnd))
                         .arg(m_processInfo.dwProcessId);
}

void ExternalAppHost::showStatus(const QString& text)
{
    m_statusLabel->setText(text);
}

#include "ShellMainWindow.h"

#include "DesktopManager.h"
#include "DiagnosticsModule.h"
#include "ExternalAppHost.h"
#include "FileManager.h"
#include "InstallerModule.h"
#include "ProgramManager.h"
#include "SettingsPage.h"
#include "ShellWindow.h"
#include "StartMenu.h"
#include "Taskbar.h"
#include "WindowManager.h"

#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QVBoxLayout>

ShellMainWindow::ShellMainWindow(QWidget* parent)
    : QWidget(parent)
    , m_desktop(new DesktopManager(this))
    , m_taskbar(new Taskbar(this))
    , m_startMenu(new StartMenu(this))
{
    setObjectName(QStringLiteral("PhoenixShell"));
    setWindowTitle(QStringLiteral("PhoenixShell Desktop"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_desktop, 1);
    layout->addWidget(m_taskbar, 0);

    connect(m_taskbar, &Taskbar::startRequested, this, &ShellMainWindow::showStartMenu);
    connect(m_taskbar, &Taskbar::workspaceRequested, m_desktop->windowManager(), &WindowManager::switchWorkspace);
    connect(m_taskbar, &Taskbar::windowButtonRequested, m_desktop->windowManager(), &WindowManager::restoreOrRaise);

    connect(m_desktop, &DesktopManager::filesRequested, this, &ShellMainWindow::openFileManager);
    connect(m_desktop, &DesktopManager::programsRequested, this, &ShellMainWindow::openProgramManager);
    connect(m_desktop, &DesktopManager::toolsRequested, this, &ShellMainWindow::openProgramManager);
    connect(m_desktop, &DesktopManager::computerRequested, this, &ShellMainWindow::openFileManager);
    connect(m_desktop, &DesktopManager::windowsSetupRequested, this, &ShellMainWindow::openInstaller);

    connect(m_startMenu, &StartMenu::filesRequested, this, &ShellMainWindow::openFileManager);
    connect(m_startMenu, &StartMenu::programsRequested, this, &ShellMainWindow::openProgramManager);
    connect(m_startMenu, &StartMenu::diagnosticsRequested, this, &ShellMainWindow::openDiagnostics);
    connect(m_startMenu, &StartMenu::windowsSetupRequested, this, &ShellMainWindow::openInstaller);
    connect(m_startMenu, &StartMenu::settingsRequested, this, &ShellMainWindow::openSettings);
    connect(m_startMenu, &StartMenu::toolRequested, this, &ShellMainWindow::launchTool);

    wireWindowManager();

    connect(&m_toolRegistry, &ToolRegistry::reloaded, this, [this]() {
        m_startMenu->setTools(m_toolRegistry.tools());
    });

    applyTheme(QStringLiteral("Dark"));
    m_toolRegistry.reload();
}

void ShellMainWindow::showStartMenu()
{
    const QPoint taskbarTopLeft = m_taskbar->mapToGlobal(QPoint(0, 0));
    m_startMenu->popupAt(taskbarTopLeft + QPoint(8, 0));
}

void ShellMainWindow::openFileManager()
{
    createShellWindow(QStringLiteral("File Manager"), new FileManager, QSize(980, 620));
}

void ShellMainWindow::openProgramManager()
{
    auto* manager = new ProgramManager(m_toolRegistry.tools());
    connect(manager, &ProgramManager::launchRequested, this, &ShellMainWindow::launchTool);
    createShellWindow(QStringLiteral("Programs"), manager, QSize(760, 520));
}

void ShellMainWindow::openDiagnostics()
{
    createShellWindow(QStringLiteral("Diagnostics"), new DiagnosticsModule, QSize(860, 620));
}

void ShellMainWindow::openInstaller()
{
    createShellWindow(QStringLiteral("Windows Setup"), new InstallerModule, QSize(880, 640));
}

void ShellMainWindow::openSettings()
{
    auto* settings = new SettingsPage;
    connect(settings, &SettingsPage::wallpaperChanged, this, [this](const QString& path) {
        m_desktop->wallpaperManager()->setWallpaper(path);
        m_desktop->update();
    });
    connect(settings, &SettingsPage::themeChanged, this, &ShellMainWindow::applyTheme);
    connect(settings, &SettingsPage::panelHeightChanged, m_taskbar, [this](int height) {
        m_taskbar->setFixedHeight(height);
    });
    createShellWindow(QStringLiteral("Settings"), settings, QSize(700, 520));
}

void ShellMainWindow::launchTool(const ToolEntry& tool)
{
    qInfo().noquote() << QStringLiteral("Launch requested: %1 -> %2").arg(tool.name, tool.path);
    auto* host = new ExternalAppHost(tool);
    connect(host, &ExternalAppHost::failed, this, [this, tool](const QString& message) {
        QMessageBox::warning(this,
                             QStringLiteral("Launch Failed"),
                             QStringLiteral("%1\n%2").arg(tool.name, message));
    });

    createShellWindow(tool.name, host, QSize(960, 640));
    host->start();
}

void ShellMainWindow::applyTheme(const QString& theme)
{
    const bool light = theme.compare(QStringLiteral("Light"), Qt::CaseInsensitive) == 0;
    const QString stylesheet = light
        ? QStringLiteral(R"(
            QWidget#PhoenixShell { background: #e8eaed; color: #202124; }
            QWidget#Taskbar { background: rgba(245, 247, 250, 235); border-top: 1px solid #b6beca; }
            QPushButton { background: #ffffff; border: 1px solid #bac3cf; border-radius: 4px; padding: 5px 9px; }
            QPushButton:hover { background: #edf4ff; }
            QPushButton:checked { background: #dce9f8; border-color: #5b86b8; }
            QWidget#StartMenu, QFrame#ShellWindow { background: #f8f9fb; border: 1px solid #9aa8b8; }
            QWidget#ShellWindowTitleBar { background: #dce3ec; }
            QLabel#ShellWindowTitle { font-weight: 600; }
            QToolButton#DesktopIcon { color: white; background: rgba(0, 0, 0, 70); border: 1px solid rgba(255,255,255,80); border-radius: 4px; }
            QToolButton#DesktopIcon:hover { background: rgba(0, 0, 0, 105); }
        )")
        : QStringLiteral(R"(
            QWidget#PhoenixShell { background: #181b20; color: #e8edf2; }
            QWidget#Taskbar { background: rgba(28, 31, 36, 238); border-top: 1px solid #3b434d; }
            QLabel { color: #e8edf2; }
            QPushButton { background: #303741; color: #f2f5f8; border: 1px solid #55616f; border-radius: 4px; padding: 5px 9px; }
            QPushButton:hover { background: #3c4652; }
            QPushButton:checked { background: #425a6a; border-color: #7ca8bc; }
            QPushButton#ShellWindowCloseButton:hover { background: #a94442; border-color: #d47a76; }
            QWidget#StartMenu, QFrame#ShellWindow { background: #22272e; border: 1px solid #687583; }
            QWidget#ShellWindowTitleBar { background: #2d343d; }
            QLabel#ShellWindowTitle { font-weight: 600; color: #f5f7fa; }
            QListWidget, QTreeView, QTextBrowser, QPlainTextEdit, QLineEdit, QComboBox, QSpinBox {
                background: #15191f; color: #eef2f5; border: 1px solid #48515e; selection-background-color: #4f6f7d;
            }
            QToolButton#DesktopIcon { color: white; background: rgba(10, 12, 15, 78); border: 1px solid rgba(255,255,255,82); border-radius: 4px; }
            QToolButton#DesktopIcon:hover { background: rgba(20, 24, 29, 128); }
        )");
    qApp->setStyleSheet(stylesheet);
}

ShellWindow* ShellMainWindow::createShellWindow(const QString& title, QWidget* content, const QSize& size)
{
    return m_desktop->windowManager()->createWindow(title, content, size);
}

void ShellMainWindow::wireWindowManager()
{
    WindowManager* manager = m_desktop->windowManager();
    connect(manager, &WindowManager::windowCreated, m_taskbar, &Taskbar::trackWindow);
    connect(manager, &WindowManager::windowClosed, m_taskbar, &Taskbar::removeWindow);
    connect(manager, &WindowManager::workspaceChanged, m_taskbar, &Taskbar::setCurrentWorkspace);
}

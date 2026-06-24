#pragma once

#include "ToolRegistry.h"

#include <QWidget>

class DesktopManager;
class ShellWindow;
class StartMenu;
class Taskbar;

class ShellMainWindow final : public QWidget {
    Q_OBJECT

public:
    explicit ShellMainWindow(QWidget* parent = nullptr);

private slots:
    void showStartMenu();
    void openFileManager();
    void openProgramManager();
    void openDiagnostics();
    void openInstaller();
    void openSettings();
    void launchTool(const ToolEntry& tool);
    void applyTheme(const QString& theme);

private:
    ShellWindow* createShellWindow(const QString& title, QWidget* content, const QSize& size);
    void wireWindowManager();

    ToolRegistry m_toolRegistry;
    DesktopManager* m_desktop = nullptr;
    Taskbar* m_taskbar = nullptr;
    StartMenu* m_startMenu = nullptr;
};

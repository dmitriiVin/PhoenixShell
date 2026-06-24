#pragma once

#include "ToolRegistry.h"

#include <QTimer>
#include <QWidget>

#include <windows.h>

class QLabel;
class QResizeEvent;
class QVBoxLayout;
class QWindow;

class ExternalAppHost final : public QWidget {
    Q_OBJECT

public:
    explicit ExternalAppHost(const ToolEntry& tool, QWidget* parent = nullptr);
    ~ExternalAppHost() override;

    void start();

protected:
    void resizeEvent(QResizeEvent* event) override;

signals:
    void failed(const QString& message);
    void processStarted(unsigned long processId);
    void processExited(unsigned long processId, unsigned long exitCode);

private slots:
    void pollForWindow();
    void pollProcessExit();

private:
    static BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam);

    HWND findProcessWindow() const;
    void embedWindow(HWND hwnd);
    void showStatus(const QString& text);

    ToolEntry m_tool;
    PROCESS_INFORMATION m_processInfo{};
    QTimer* m_windowPollTimer = nullptr;
    QTimer* m_processPollTimer = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QLabel* m_statusLabel = nullptr;
    QWindow* m_foreignWindow = nullptr;
    QWidget* m_windowContainer = nullptr;
    HWND m_embeddedHwnd = nullptr;
    int m_pollAttempts = 0;
    bool m_started = false;
};

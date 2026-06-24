#pragma once

#include <QHash>
#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;
class QHBoxLayout;
class ShellWindow;

class Taskbar final : public QWidget {
    Q_OBJECT

public:
    explicit Taskbar(QWidget* parent = nullptr);

    void trackWindow(ShellWindow* window, const QString& title);
    void removeWindow(ShellWindow* window);
    void setCurrentWorkspace(int workspace);

signals:
    void startRequested();
    void workspaceRequested(int workspace);
    void windowButtonRequested(ShellWindow* window);

private slots:
    void refreshStatus();

private:
    QString networkSummary() const;
    QString diskSummary() const;

    QPushButton* m_startButton = nullptr;
    QPushButton* m_workspaceButtons[3] = {};
    QWidget* m_taskArea = nullptr;
    QHBoxLayout* m_taskLayout = nullptr;
    QLabel* m_networkLabel = nullptr;
    QLabel* m_diskLabel = nullptr;
    QLabel* m_clockLabel = nullptr;
    QTimer* m_timer = nullptr;
    QHash<ShellWindow*, QPushButton*> m_windowButtons;
};

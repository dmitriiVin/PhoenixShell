#pragma once

#include "ShellWindow.h"

#include <QObject>
#include <QHash>
#include <QPoint>
#include <QSize>

class WindowManager final : public QObject {
    Q_OBJECT

public:
    explicit WindowManager(QWidget* host, QObject* parent = nullptr);

    ShellWindow* createWindow(const QString& title, QWidget* content, const QSize& preferredSize, int workspace = -1);
    int currentWorkspace() const;

public slots:
    void switchWorkspace(int workspace);
    void restoreOrRaise(ShellWindow* window);

signals:
    void windowCreated(ShellWindow* window, const QString& title);
    void windowClosed(ShellWindow* window);
    void windowActivated(ShellWindow* window);
    void workspaceChanged(int workspace);

private:
    struct ManagedWindow {
        QString title;
        int workspace = 0;
    };

    void applyWorkspaceVisibility();
    QPoint nextWindowPosition(const QSize& size) const;

    QWidget* m_host = nullptr;
    QHash<ShellWindow*, ManagedWindow> m_windows;
    int m_currentWorkspace = 0;
    int m_cascadeOffset = 0;
};

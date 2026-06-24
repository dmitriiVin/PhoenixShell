#include "WindowManager.h"

#include <QSize>

WindowManager::WindowManager(QWidget* host, QObject* parent)
    : QObject(parent)
    , m_host(host)
{
}

ShellWindow* WindowManager::createWindow(const QString& title, QWidget* content, const QSize& preferredSize, int workspace)
{
    if (workspace < 0) {
        workspace = m_currentWorkspace;
    }

    auto* window = new ShellWindow(title, content, m_host);
    const QSize size(preferredSize.width() > 0 ? preferredSize.width() : 800,
                     preferredSize.height() > 0 ? preferredSize.height() : 520);
    window->resize(size.boundedTo(m_host->size() - QSize(16, 16)));
    window->move(nextWindowPosition(window->size()));
    ++m_cascadeOffset;

    m_windows.insert(window, ManagedWindow{title, workspace});

    connect(window, &ShellWindow::closed, this, [this](ShellWindow* closedWindow) {
        m_windows.remove(closedWindow);
        emit windowClosed(closedWindow);
    });
    connect(window, &ShellWindow::activated, this, &WindowManager::windowActivated);

    window->show();
    window->raise();
    applyWorkspaceVisibility();
    emit windowCreated(window, title);
    return window;
}

int WindowManager::currentWorkspace() const
{
    return m_currentWorkspace;
}

void WindowManager::switchWorkspace(int workspace)
{
    if (workspace < 0 || workspace > 2 || workspace == m_currentWorkspace) {
        return;
    }

    m_currentWorkspace = workspace;
    applyWorkspaceVisibility();
    emit workspaceChanged(workspace);
}

void WindowManager::restoreOrRaise(ShellWindow* window)
{
    if (!window || !m_windows.contains(window)) {
        return;
    }

    const int workspace = m_windows.value(window).workspace;
    if (workspace != m_currentWorkspace) {
        switchWorkspace(workspace);
    }

    window->restoreWindow();
}

void WindowManager::applyWorkspaceVisibility()
{
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        ShellWindow* window = it.key();
        if (it.value().workspace == m_currentWorkspace && !window->isMinimized()) {
            window->show();
        } else {
            window->hide();
        }
    }
}

QPoint WindowManager::nextWindowPosition(const QSize& size) const
{
    const int offset = 28 * (m_cascadeOffset % 8);
    QPoint pos(70 + offset, 50 + offset);
    if (m_host) {
        pos.setX(qMin(pos.x(), qMax(0, m_host->width() - size.width() - 16)));
        pos.setY(qMin(pos.y(), qMax(0, m_host->height() - size.height() - 16)));
    }
    return pos;
}

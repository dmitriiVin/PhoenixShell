#pragma once

#include "WallpaperManager.h"
#include "WindowManager.h"

#include <QWidget>
#include <QVector>

class QToolButton;
class QPaintEvent;
class QResizeEvent;

class DesktopManager final : public QWidget {
    Q_OBJECT

public:
    explicit DesktopManager(QWidget* parent = nullptr);

    WindowManager* windowManager() const;
    WallpaperManager* wallpaperManager();

signals:
    void filesRequested();
    void programsRequested();
    void toolsRequested();
    void computerRequested();
    void windowsSetupRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QToolButton* createDesktopIcon(const QString& text, const QIcon& icon);
    void layoutIcons();

    WallpaperManager m_wallpaper;
    WindowManager* m_windowManager = nullptr;
    QVector<QToolButton*> m_icons;
};

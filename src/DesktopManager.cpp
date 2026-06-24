#include "DesktopManager.h"

#include <QApplication>
#include <QPainter>
#include <QResizeEvent>
#include <QStyle>
#include <QToolButton>

DesktopManager::DesktopManager(QWidget* parent)
    : QWidget(parent)
    , m_windowManager(new WindowManager(this, this))
{
    setObjectName(QStringLiteral("DesktopManager"));
    setAutoFillBackground(false);
    setMouseTracking(true);

    auto* setup = createDesktopIcon(QStringLiteral("Windows Setup"),
                                    style()->standardIcon(QStyle::SP_ComputerIcon));
    auto* programs = createDesktopIcon(QStringLiteral("Programs"),
                                       style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    auto* tools = createDesktopIcon(QStringLiteral("Tools"),
                                    style()->standardIcon(QStyle::SP_FileDialogContentsView));
    auto* computer = createDesktopIcon(QStringLiteral("Computer"),
                                       style()->standardIcon(QStyle::SP_DriveHDIcon));
    auto* files = createDesktopIcon(QStringLiteral("Files"),
                                    style()->standardIcon(QStyle::SP_DirIcon));

    connect(setup, &QToolButton::clicked, this, &DesktopManager::windowsSetupRequested);
    connect(programs, &QToolButton::clicked, this, &DesktopManager::programsRequested);
    connect(tools, &QToolButton::clicked, this, &DesktopManager::toolsRequested);
    connect(computer, &QToolButton::clicked, this, &DesktopManager::computerRequested);
    connect(files, &QToolButton::clicked, this, &DesktopManager::filesRequested);
}

WindowManager* DesktopManager::windowManager() const
{
    return m_windowManager;
}

WallpaperManager* DesktopManager::wallpaperManager()
{
    return &m_wallpaper;
}

void DesktopManager::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    m_wallpaper.paint(&painter, rect());
}

void DesktopManager::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutIcons();
}

QToolButton* DesktopManager::createDesktopIcon(const QString& text, const QIcon& icon)
{
    auto* button = new QToolButton(this);
    button->setText(text);
    button->setIcon(icon);
    button->setIconSize(QSize(34, 34));
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setFixedSize(96, 82);
    button->setAutoRaise(true);
    button->setObjectName(QStringLiteral("DesktopIcon"));
    m_icons.push_back(button);
    return button;
}

void DesktopManager::layoutIcons()
{
    constexpr int left = 18;
    constexpr int top = 18;
    constexpr int spacing = 12;
    int y = top;
    for (QToolButton* icon : m_icons) {
        icon->move(left, y);
        icon->raise();
        y += icon->height() + spacing;
    }
}

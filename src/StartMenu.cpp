#include "StartMenu.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QScreen>
#include <QGuiApplication>

StartMenu::StartMenu(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
    , m_categories(new QListWidget(this))
    , m_items(new QListWidget(this))
{
    setObjectName(QStringLiteral("StartMenu"));
    setFixedSize(560, 420);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    layout->addWidget(m_categories, 0);
    layout->addWidget(m_items, 1);

    m_categories->setFixedWidth(170);
    m_categories->addItems({
        QStringLiteral("Windows Setup"),
        QStringLiteral("Programs"),
        QStringLiteral("Diagnostics"),
        QStringLiteral("Recovery"),
        QStringLiteral("Tools"),
        QStringLiteral("Settings")
    });
    m_categories->setCurrentRow(0);

    connect(m_categories, &QListWidget::currentRowChanged, this, &StartMenu::populateCategory);
    connect(m_items, &QListWidget::itemActivated, this, &StartMenu::activateCurrentItem);
    connect(m_items, &QListWidget::itemClicked, this, &StartMenu::activateCurrentItem);

    populateCategory();
}

void StartMenu::setTools(const QVector<ToolEntry>& tools)
{
    m_tools = tools;
    populateCategory();
}

void StartMenu::popupAt(const QPoint& globalBottomLeft)
{
    QPoint pos(globalBottomLeft.x(), globalBottomLeft.y() - height());
    if (const QScreen* screen = QGuiApplication::screenAt(globalBottomLeft)) {
        const QRect available = screen->availableGeometry();
        if (pos.x() + width() > available.right()) {
            pos.setX(available.right() - width());
        }
        if (pos.y() < available.top()) {
            pos.setY(available.top());
        }
    }

    move(pos);
    show();
    raise();
    activateWindow();
}

void StartMenu::populateCategory()
{
    if (!m_items || !m_categories) {
        return;
    }

    m_items->clear();
    m_visibleTools.clear();

    const QString category = m_categories->currentItem()
        ? m_categories->currentItem()->text()
        : QStringLiteral("Windows Setup");

    if (category == QStringLiteral("Windows Setup")) {
        addBuiltinItem(QStringLiteral("Install Windows"), QStringLiteral("windowsSetup"));
        return;
    }

    if (category == QStringLiteral("Programs")) {
        addBuiltinItem(QStringLiteral("Program Manager"), QStringLiteral("programs"));
    }

    if (category == QStringLiteral("Diagnostics")) {
        addBuiltinItem(QStringLiteral("System Diagnostics"), QStringLiteral("diagnostics"));
    }

    if (category == QStringLiteral("Recovery")) {
        addBuiltinItem(QStringLiteral("File Manager"), QStringLiteral("files"));
    }

    if (category == QStringLiteral("Tools")) {
        addBuiltinItem(QStringLiteral("File Manager"), QStringLiteral("files"));
    }

    if (category == QStringLiteral("Settings")) {
        addBuiltinItem(QStringLiteral("Settings"), QStringLiteral("settings"));
        return;
    }

    for (const ToolEntry& tool : m_tools) {
        const bool categoryMatch = tool.category.compare(category, Qt::CaseInsensitive) == 0
            || (category == QStringLiteral("Programs") && tool.category.compare(QStringLiteral("Programs"), Qt::CaseInsensitive) == 0)
            || (category == QStringLiteral("Tools") && tool.category.compare(QStringLiteral("Diagnostics"), Qt::CaseInsensitive) != 0);
        if (categoryMatch) {
            addToolItem(tool);
        }
    }
}

void StartMenu::activateCurrentItem(QListWidgetItem* item)
{
    if (!item) {
        return;
    }

    const QString kind = item->data(Qt::UserRole).toString();
    hide();

    if (kind == QStringLiteral("tool")) {
        const int index = item->data(Qt::UserRole + 1).toInt();
        if (index >= 0 && index < m_visibleTools.size()) {
            emit toolRequested(m_visibleTools.at(index));
        }
        return;
    }

    if (kind == QStringLiteral("windowsSetup")) {
        emit windowsSetupRequested();
    } else if (kind == QStringLiteral("programs")) {
        emit programsRequested();
    } else if (kind == QStringLiteral("diagnostics")) {
        emit diagnosticsRequested();
    } else if (kind == QStringLiteral("files")) {
        emit filesRequested();
    } else if (kind == QStringLiteral("settings")) {
        emit settingsRequested();
    } else if (kind == QStringLiteral("recovery")) {
        emit recoveryRequested();
    }
}

void StartMenu::addBuiltinItem(const QString& title, const QString& id)
{
    auto* item = new QListWidgetItem(title, m_items);
    item->setData(Qt::UserRole, id);
}

void StartMenu::addToolItem(const ToolEntry& tool)
{
    auto* item = new QListWidgetItem(tool.name, m_items);
    item->setToolTip(tool.path);
    item->setData(Qt::UserRole, QStringLiteral("tool"));
    item->setData(Qt::UserRole + 1, m_visibleTools.size());
    m_visibleTools.push_back(tool);
}

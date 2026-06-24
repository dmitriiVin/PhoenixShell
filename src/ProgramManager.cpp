#include "ProgramManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

ProgramManager::ProgramManager(const QVector<ToolEntry>& tools, QWidget* parent)
    : QWidget(parent)
    , m_list(new QListWidget(this))
    , m_tools(tools)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* header = new QLabel(QStringLiteral("Local tools from DATA/Tools and config/tools.json"), this);
    layout->addWidget(header);
    layout->addWidget(m_list, 1);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* launchButton = new QPushButton(QStringLiteral("Launch"), this);
    buttons->addWidget(launchButton);
    layout->addLayout(buttons);

    for (int i = 0; i < m_tools.size(); ++i) {
        const ToolEntry& tool = m_tools.at(i);
        auto* item = new QListWidgetItem(QStringLiteral("%1  [%2]").arg(tool.name, tool.category), m_list);
        item->setToolTip(tool.path);
        item->setData(Qt::UserRole, i);
    }

    connect(launchButton, &QPushButton::clicked, this, &ProgramManager::launchSelected);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &ProgramManager::activateItem);
}

void ProgramManager::launchSelected()
{
    activateItem(m_list->currentItem());
}

void ProgramManager::activateItem(QListWidgetItem* item)
{
    if (!item) {
        return;
    }

    const int index = item->data(Qt::UserRole).toInt();
    if (index >= 0 && index < m_tools.size()) {
        emit launchRequested(m_tools.at(index));
    }
}

#pragma once

#include "ToolRegistry.h"

#include <QWidget>
#include <QVector>

class QListWidget;
class QListWidgetItem;

class ProgramManager final : public QWidget {
    Q_OBJECT

public:
    explicit ProgramManager(const QVector<ToolEntry>& tools, QWidget* parent = nullptr);

signals:
    void launchRequested(const ToolEntry& tool);

private slots:
    void launchSelected();
    void activateItem(QListWidgetItem* item);

private:
    QListWidget* m_list = nullptr;
    QVector<ToolEntry> m_tools;
};

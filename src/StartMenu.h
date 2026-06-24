#pragma once

#include "ToolRegistry.h"

#include <QPoint>
#include <QWidget>
#include <QVector>

class QListWidget;
class QListWidgetItem;

class StartMenu final : public QWidget {
    Q_OBJECT

public:
    explicit StartMenu(QWidget* parent = nullptr);

    void setTools(const QVector<ToolEntry>& tools);
    void popupAt(const QPoint& globalBottomLeft);

signals:
    void filesRequested();
    void programsRequested();
    void diagnosticsRequested();
    void recoveryRequested();
    void settingsRequested();
    void windowsSetupRequested();
    void toolRequested(const ToolEntry& tool);

private slots:
    void populateCategory();
    void activateCurrentItem(QListWidgetItem* item);

private:
    void addBuiltinItem(const QString& title, const QString& id);
    void addToolItem(const ToolEntry& tool);

    QListWidget* m_categories = nullptr;
    QListWidget* m_items = nullptr;
    QVector<ToolEntry> m_tools;
    QVector<ToolEntry> m_visibleTools;
};

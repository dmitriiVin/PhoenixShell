#pragma once

#include <QObject>
#include <QString>
#include <QVector>

struct ToolEntry {
    QString name;
    QString path;
    QString iconPath;
    QString category;
    QString workingDirectory;

    bool isValid() const;
};

Q_DECLARE_METATYPE(ToolEntry)

class ToolRegistry final : public QObject {
    Q_OBJECT

public:
    explicit ToolRegistry(QObject* parent = nullptr);

    void reload();
    QVector<ToolEntry> tools() const;
    QVector<ToolEntry> toolsForCategory(const QString& category) const;

signals:
    void reloaded();

private:
    QVector<ToolEntry> readConfiguredTools() const;
    QVector<ToolEntry> scanToolDirectories() const;
    void addUnique(QVector<ToolEntry>& list, const ToolEntry& entry) const;

    QVector<ToolEntry> m_tools;
};

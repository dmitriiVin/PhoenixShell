#pragma once

#include <QWidget>

class QFileSystemModel;
class QTreeView;
class QComboBox;

class FileManager final : public QWidget {
    Q_OBJECT

public:
    explicit FileManager(QWidget* parent = nullptr);

private slots:
    void setRootFromDrive(int index);
    void createFolder();
    void deleteSelected();
    void renameSelected();
    void copySelected();
    void moveSelected();
    void refreshDrives();

private:
    QString selectedPath() const;
    bool copyRecursively(const QString& sourcePath, const QString& destinationPath);
    bool removeRecursively(const QString& path);

    QFileSystemModel* m_model = nullptr;
    QTreeView* m_view = nullptr;
    QComboBox* m_driveCombo = nullptr;
};

#include "FileManager.h"

#include <QComboBox>
#include <QAbstractItemView>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>

FileManager::FileManager(QWidget* parent)
    : QWidget(parent)
    , m_model(new QFileSystemModel(this))
    , m_view(new QTreeView(this))
    , m_driveCombo(new QComboBox(this))
{
    m_model->setRootPath(QString());
    m_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs);
    m_model->setReadOnly(false);

    m_view->setModel(m_model);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setSortingEnabled(true);
    m_view->sortByColumn(0, Qt::AscendingOrder);

    auto* toolbar = new QHBoxLayout;
    auto* newFolderButton = new QPushButton(QStringLiteral("New Folder"), this);
    auto* copyButton = new QPushButton(QStringLiteral("Copy"), this);
    auto* moveButton = new QPushButton(QStringLiteral("Move"), this);
    auto* renameButton = new QPushButton(QStringLiteral("Rename"), this);
    auto* deleteButton = new QPushButton(QStringLiteral("Delete"), this);
    auto* refreshButton = new QPushButton(QStringLiteral("Refresh"), this);

    toolbar->addWidget(m_driveCombo);
    toolbar->addWidget(newFolderButton);
    toolbar->addWidget(copyButton);
    toolbar->addWidget(moveButton);
    toolbar->addWidget(renameButton);
    toolbar->addWidget(deleteButton);
    toolbar->addWidget(refreshButton);
    toolbar->addStretch(1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addLayout(toolbar);
    layout->addWidget(m_view, 1);

    connect(m_driveCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &FileManager::setRootFromDrive);
    connect(newFolderButton, &QPushButton::clicked, this, &FileManager::createFolder);
    connect(copyButton, &QPushButton::clicked, this, &FileManager::copySelected);
    connect(moveButton, &QPushButton::clicked, this, &FileManager::moveSelected);
    connect(renameButton, &QPushButton::clicked, this, &FileManager::renameSelected);
    connect(deleteButton, &QPushButton::clicked, this, &FileManager::deleteSelected);
    connect(refreshButton, &QPushButton::clicked, this, &FileManager::refreshDrives);

    refreshDrives();
}

void FileManager::setRootFromDrive(int index)
{
    const QString path = m_driveCombo->itemData(index).toString();
    if (path.isEmpty()) {
        return;
    }

    m_view->setRootIndex(m_model->index(path));
}

void FileManager::createFolder()
{
    QModelIndex parentIndex = m_view->currentIndex();
    QString parentPath = m_model->fileInfo(parentIndex).isDir()
        ? m_model->filePath(parentIndex)
        : m_model->fileInfo(parentIndex).absolutePath();

    if (parentPath.isEmpty()) {
        parentPath = m_driveCombo->currentData().toString();
    }

    const QString name = QInputDialog::getText(this, QStringLiteral("New Folder"), QStringLiteral("Folder name:")).trimmed();
    if (name.isEmpty()) {
        return;
    }

    if (!QDir(parentPath).mkdir(name)) {
        QMessageBox::warning(this, QStringLiteral("New Folder"), QStringLiteral("Could not create folder."));
    }
}

void FileManager::deleteSelected()
{
    const QString path = selectedPath();
    if (path.isEmpty()) {
        return;
    }

    if (QMessageBox::question(this,
                              QStringLiteral("Delete"),
                              QStringLiteral("Delete selected item?\n%1").arg(QDir::toNativeSeparators(path)))
        != QMessageBox::Yes) {
        return;
    }

    if (!removeRecursively(path)) {
        QMessageBox::warning(this, QStringLiteral("Delete"), QStringLiteral("Could not delete selected item."));
    }
}

void FileManager::renameSelected()
{
    const QString path = selectedPath();
    if (path.isEmpty()) {
        return;
    }

    const QFileInfo info(path);
    const QString name = QInputDialog::getText(this,
                                               QStringLiteral("Rename"),
                                               QStringLiteral("New name:"),
                                               QLineEdit::Normal,
                                               info.fileName()).trimmed();
    if (name.isEmpty() || name == info.fileName()) {
        return;
    }

    if (!QDir(info.absolutePath()).rename(info.fileName(), name)) {
        QMessageBox::warning(this, QStringLiteral("Rename"), QStringLiteral("Could not rename selected item."));
    }
}

void FileManager::copySelected()
{
    const QString path = selectedPath();
    if (path.isEmpty()) {
        return;
    }

    const QString destinationDir = QFileDialog::getExistingDirectory(this, QStringLiteral("Copy To"));
    if (destinationDir.isEmpty()) {
        return;
    }

    const QFileInfo info(path);
    const QString destination = QDir(destinationDir).filePath(info.fileName());
    if (!copyRecursively(path, destination)) {
        QMessageBox::warning(this, QStringLiteral("Copy"), QStringLiteral("Copy failed."));
    }
}

void FileManager::moveSelected()
{
    const QString path = selectedPath();
    if (path.isEmpty()) {
        return;
    }

    const QString destinationDir = QFileDialog::getExistingDirectory(this, QStringLiteral("Move To"));
    if (destinationDir.isEmpty()) {
        return;
    }

    const QFileInfo info(path);
    const QString destination = QDir(destinationDir).filePath(info.fileName());
    if (!QFile::rename(path, destination)) {
        if (!copyRecursively(path, destination) || !removeRecursively(path)) {
            QMessageBox::warning(this, QStringLiteral("Move"), QStringLiteral("Move failed."));
        }
    }
}

void FileManager::refreshDrives()
{
    m_driveCombo->blockSignals(true);
    m_driveCombo->clear();
    for (const QFileInfo& drive : QDir::drives()) {
        const QString path = drive.absoluteFilePath();
        m_driveCombo->addItem(QDir::toNativeSeparators(path), path);
    }
    m_driveCombo->blockSignals(false);

    if (m_driveCombo->count() > 0) {
        m_driveCombo->setCurrentIndex(0);
        setRootFromDrive(0);
    }
}

QString FileManager::selectedPath() const
{
    const QModelIndex index = m_view->currentIndex();
    if (!index.isValid()) {
        return {};
    }

    return m_model->filePath(index);
}

bool FileManager::copyRecursively(const QString& sourcePath, const QString& destinationPath)
{
    const QFileInfo sourceInfo(sourcePath);
    if (sourceInfo.isDir()) {
        QDir destinationDir(destinationPath);
        if (!destinationDir.exists() && !QDir().mkpath(destinationPath)) {
            return false;
        }

        QDir sourceDir(sourcePath);
        const QFileInfoList children = sourceDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
        for (const QFileInfo& child : children) {
            if (!copyRecursively(child.absoluteFilePath(), destinationDir.filePath(child.fileName()))) {
                return false;
            }
        }
        return true;
    }

    if (QFileInfo::exists(destinationPath)) {
        QFile::remove(destinationPath);
    }
    return QFile::copy(sourcePath, destinationPath);
}

bool FileManager::removeRecursively(const QString& path)
{
    const QFileInfo info(path);
    if (info.isDir()) {
        return QDir(path).removeRecursively();
    }

    return QFile::remove(path);
}

#pragma once

#include "DataLoader.h"

#include <QProcess>
#include <QVector>
#include <QWidget>
#include <QLineEdit>
#include <memory>

class QCheckBox;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QWidget;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

class InstallerModule final : public QWidget {
    Q_OBJECT

public:
    explicit InstallerModule(QWidget* parent = nullptr);

private slots:
    void refreshDisks();
    void refreshDataSource();
    void browseImage();
    void updateStartState();
    void startInstall();
    void handleProcessOutput();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void handleProcessError(QProcess::ProcessError error);
    void syncSelectedImage(int index);

private:
    struct Command {
        QString program;
        QStringList arguments;
        QString title;
    };

    struct ManifestEntry {
        QString name;
        QString sourcePath;
        QString targetPath;
        QString category;
        QString silentArguments;
    };

    void appendLog(const QString& text);
    void runNextCommand();
    void setDataSourceLabel(const QString& text);
    void populateWindowsImages();
    void populateDriverList();
    void populateProgramList();
    QString writeDiskPartScript(int diskNumber);
    QString writeManifestFile(const QVector<ManifestEntry>& entries, const QString& windowsRootPath);
    QString writeSetupCompleteScript(const QString& windowsRootPath);
    QString writeCopyBatch(const QString& title, const QString& source, const QString& destination);
    QString findSelectedWindowsImagePath() const;
    QString findInstallerAgentExecutable() const;
    QVector<DriverData> selectedDrivers() const;
    QVector<ToolData> selectedPrograms() const;
    QVector<ManifestEntry> buildProgramManifest(const QString& windowsRootPath) const;
    QVector<ManifestEntry> buildDriverManifest(const QString& windowsRootPath) const;
    QListWidgetItem* createCheckItem(QListWidget* parent, const QString& text, const QString& toolTip, int index);
    void enableSelectionWidgets(bool enabled);

    QComboBox* m_diskCombo = nullptr;
    QComboBox* m_windowsImagesCombo = nullptr;
    QLineEdit* m_imagePath = nullptr;
    QLabel* m_dataSourceLabel = nullptr;
    QSpinBox* m_imageIndex = nullptr;
    QListWidget* m_driverList = nullptr;
    QListWidget* m_programList = nullptr;
    QCheckBox* m_confirm = nullptr;
    QCheckBox* m_rebootAfterSuccess = nullptr;
    QPushButton* m_startButton = nullptr;
    QPushButton* m_refreshDataButton = nullptr;
    QPlainTextEdit* m_log = nullptr;
    QProcess* m_process = nullptr;
    QVector<Command> m_queue;
    QString m_diskpartScriptPath;
    QString m_manifestScriptPath;
    QString m_setupCompleteScriptPath;
    QString m_dataPartitionLetter;
    QString m_dataRootPath;
    QString m_installerAgentPath;
    std::unique_ptr<DataLoader> m_dataLoader;
    QVector<ToolData> m_tools;
    QVector<DriverData> m_drivers;
    QVector<WindowsImageData> m_windowsImages;
};

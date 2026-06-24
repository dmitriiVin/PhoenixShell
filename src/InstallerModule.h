#pragma once

#include <QProcess>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

class InstallerModule final : public QWidget {
    Q_OBJECT

public:
    explicit InstallerModule(QWidget* parent = nullptr);

private slots:
    void refreshDisks();
    void browseImage();
    void updateStartState();
    void startInstall();
    void handleProcessOutput();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void handleProcessError(QProcess::ProcessError error);

private:
    struct Command {
        QString program;
        QStringList arguments;
        QString title;
    };

    void appendLog(const QString& text);
    void runNextCommand();
    QString writeDiskPartScript(int diskNumber);

    QComboBox* m_diskCombo = nullptr;
    QLineEdit* m_imagePath = nullptr;
    QSpinBox* m_imageIndex = nullptr;
    QCheckBox* m_confirm = nullptr;
    QCheckBox* m_rebootAfterSuccess = nullptr;
    QPushButton* m_startButton = nullptr;
    QPlainTextEdit* m_log = nullptr;
    QProcess* m_process = nullptr;
    QVector<Command> m_queue;
    QString m_diskpartScriptPath;
};

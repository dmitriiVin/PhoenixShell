#include "InstallerModule.h"

#include "PathUtils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryFile>
#include <QTextStream>
#include <QVBoxLayout>

#include <windows.h>

InstallerModule::InstallerModule(QWidget* parent)
    : QWidget(parent)
    , m_diskCombo(new QComboBox(this))
    , m_imagePath(new QLineEdit(this))
    , m_imageIndex(new QSpinBox(this))
    , m_confirm(new QCheckBox(QStringLiteral("I understand this will erase the selected disk"), this))
    , m_rebootAfterSuccess(new QCheckBox(QStringLiteral("Reboot after successful install"), this))
    , m_startButton(new QPushButton(QStringLiteral("Start Install"), this))
    , m_log(new QPlainTextEdit(this))
    , m_process(new QProcess(this))
{
    m_imagePath->setText(PathUtils::resolveLocalPath(QStringLiteral("Windows/install.esd")));
    m_imageIndex->setRange(1, 99);
    m_imageIndex->setValue(1);
    m_log->setReadOnly(true);
    m_startButton->setEnabled(false);

    auto* imageBrowse = new QPushButton(QStringLiteral("Browse"), this);
    auto* refreshButton = new QPushButton(QStringLiteral("Refresh Disks"), this);

    auto* imageRow = new QHBoxLayout;
    imageRow->addWidget(m_imagePath, 1);
    imageRow->addWidget(imageBrowse);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Target disk:"), m_diskCombo);
    form->addRow(QStringLiteral("Windows image:"), imageRow);
    form->addRow(QStringLiteral("Image index:"), m_imageIndex);

    auto* controls = new QHBoxLayout;
    controls->addWidget(refreshButton);
    controls->addStretch(1);
    controls->addWidget(m_startButton);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->addLayout(form);
    layout->addWidget(m_confirm);
    layout->addWidget(m_rebootAfterSuccess);
    layout->addLayout(controls);
    layout->addWidget(m_log, 1);

    connect(refreshButton, &QPushButton::clicked, this, &InstallerModule::refreshDisks);
    connect(imageBrowse, &QPushButton::clicked, this, &InstallerModule::browseImage);
    connect(m_confirm, &QCheckBox::toggled, this, &InstallerModule::updateStartState);
    connect(m_imagePath, &QLineEdit::textChanged, this, &InstallerModule::updateStartState);
    connect(m_startButton, &QPushButton::clicked, this, &InstallerModule::startInstall);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &InstallerModule::handleProcessOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &InstallerModule::handleProcessOutput);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &InstallerModule::handleProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &InstallerModule::handleProcessError);

    refreshDisks();
    updateStartState();
}

void InstallerModule::refreshDisks()
{
    m_diskCombo->clear();
    for (int i = 0; i < 32; ++i) {
        const QString devicePath = QStringLiteral("\\\\.\\PhysicalDrive%1").arg(i);
        const std::wstring path = devicePath.toStdWString();
        HANDLE handle = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            continue;
        }

        m_diskCombo->addItem(QStringLiteral("Disk %1").arg(i), i);
        CloseHandle(handle);
    }

    if (m_diskCombo->count() == 0) {
        m_diskCombo->addItem(QStringLiteral("Disk 0"), 0);
        appendLog(QStringLiteral("No PhysicalDrive handles opened; defaulting to Disk 0."));
    }
}

void InstallerModule::browseImage()
{
    const QString file = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("Windows image"),
                                                      PathUtils::dataRoot(),
                                                      QStringLiteral("Windows Images (*.esd *.wim)"));
    if (!file.isEmpty()) {
        m_imagePath->setText(file);
    }
}

void InstallerModule::updateStartState()
{
    m_startButton->setEnabled(m_confirm->isChecked()
                              && QFileInfo::exists(m_imagePath->text())
                              && m_process->state() == QProcess::NotRunning);
}

void InstallerModule::startInstall()
{
    const int diskNumber = m_diskCombo->currentData().toInt();
    if (QMessageBox::warning(this,
                             QStringLiteral("Confirm Install"),
                             QStringLiteral("Disk %1 will be erased and Windows will be applied from:\n%2")
                                 .arg(diskNumber)
                                 .arg(QDir::toNativeSeparators(m_imagePath->text())),
                             QMessageBox::Cancel | QMessageBox::Yes,
                             QMessageBox::Cancel)
        != QMessageBox::Yes) {
        return;
    }

    m_log->clear();
    m_queue.clear();

    m_diskpartScriptPath = writeDiskPartScript(diskNumber);
    if (m_diskpartScriptPath.isEmpty()) {
        appendLog(QStringLiteral("Failed to create DiskPart script."));
        return;
    }

    m_queue.push_back({QStringLiteral("diskpart"), {QStringLiteral("/s"), m_diskpartScriptPath}, QStringLiteral("Partition disk")});
    m_queue.push_back({QStringLiteral("dism"),
                       {QStringLiteral("/Apply-Image"),
                        QStringLiteral("/ImageFile:%1").arg(QDir::toNativeSeparators(m_imagePath->text())),
                        QStringLiteral("/Index:%1").arg(m_imageIndex->value()),
                        QStringLiteral("/ApplyDir:W:\\")},
                       QStringLiteral("Apply Windows image")});
    m_queue.push_back({QStringLiteral("bcdboot"),
                       {QStringLiteral("W:\\Windows"), QStringLiteral("/s"), QStringLiteral("S:"), QStringLiteral("/f"), QStringLiteral("UEFI")},
                       QStringLiteral("Create bootloader")});

    const QString installerAgent = PathUtils::resolveLocalPath(QStringLiteral("InstallerAgent"));
    if (QFileInfo::exists(installerAgent)) {
        m_queue.push_back({QStringLiteral("xcopy"),
                           {QDir::toNativeSeparators(installerAgent),
                            QStringLiteral("W:\\InstallerAgent\\"),
                            QStringLiteral("/E"),
                            QStringLiteral("/I"),
                            QStringLiteral("/Y")},
                           QStringLiteral("Copy InstallerAgent")});
    }

    if (m_rebootAfterSuccess->isChecked()) {
        m_queue.push_back({QStringLiteral("wpeutil"), {QStringLiteral("reboot")}, QStringLiteral("Reboot")});
    }

    m_startButton->setEnabled(false);
    runNextCommand();
}

void InstallerModule::handleProcessOutput()
{
    const QString stdoutText = QString::fromLocal8Bit(m_process->readAllStandardOutput());
    const QString stderrText = QString::fromLocal8Bit(m_process->readAllStandardError());
    if (!stdoutText.isEmpty()) {
        appendLog(stdoutText.trimmed());
    }
    if (!stderrText.isEmpty()) {
        appendLog(stderrText.trimmed());
    }
}

void InstallerModule::handleProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    handleProcessOutput();
    if (status != QProcess::NormalExit || exitCode != 0) {
        appendLog(QStringLiteral("Command failed. exit=%1 status=%2").arg(exitCode).arg(status));
        m_queue.clear();
        updateStartState();
        return;
    }

    appendLog(QStringLiteral("Command completed."));
    runNextCommand();
}

void InstallerModule::handleProcessError(QProcess::ProcessError error)
{
    appendLog(QStringLiteral("Process error %1: %2").arg(error).arg(m_process->errorString()));
    m_queue.clear();
    updateStartState();
}

void InstallerModule::appendLog(const QString& text)
{
    if (text.isEmpty()) {
        return;
    }

    m_log->appendPlainText(text);
    qInfo().noquote() << QStringLiteral("[Installer] %1").arg(text);
}

void InstallerModule::runNextCommand()
{
    if (m_queue.isEmpty()) {
        appendLog(QStringLiteral("Install sequence completed."));
        updateStartState();
        return;
    }

    const Command command = m_queue.takeFirst();
    appendLog(QStringLiteral("== %1 ==").arg(command.title));
    appendLog(command.program + QLatin1Char(' ') + command.arguments.join(QLatin1Char(' ')));
    m_process->start(command.program, command.arguments);
}

QString InstallerModule::writeDiskPartScript(int diskNumber)
{
    QTemporaryFile file(QDir::tempPath() + QStringLiteral("/phoenix_diskpart_XXXXXX.txt"));
    file.setAutoRemove(false);
    if (!file.open()) {
        return {};
    }

    QTextStream stream(&file);
    stream << "select disk " << diskNumber << "\r\n";
    stream << "clean\r\n";
    stream << "convert gpt\r\n";
    stream << "create partition efi size=100\r\n";
    stream << "format quick fs=fat32 label=System\r\n";
    stream << "assign letter=S\r\n";
    stream << "create partition primary\r\n";
    stream << "format quick fs=ntfs label=Windows\r\n";
    stream << "assign letter=W\r\n";
    stream << "exit\r\n";
    file.close();
    return file.fileName();
}

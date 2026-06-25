#include "InstallerModule.h"

#include "ApplicationLauncher.h"
#include "PartitionScanner.h"
#include "PathUtils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QAbstractItemView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryFile>
#include <QTextStream>
#include <QVBoxLayout>

#include <windows.h>
#include <optional>

namespace {

QString categoryLabelForDriver(const DriverData& driver)
{
    if (!driver.category.isEmpty()) {
        return driver.category;
    }

    return QStringLiteral("Drivers");
}

QString windowsImageDescription(const WindowsImageData& image)
{
    return QStringLiteral("%1 [%2 %3]").arg(image.fileName, image.version, image.edition);
}

} // namespace

InstallerModule::InstallerModule(QWidget* parent)
    : QWidget(parent)
    , m_diskCombo(new QComboBox(this))
    , m_windowsImagesCombo(new QComboBox(this))
    , m_imagePath(new QLineEdit(this))
     , m_dataSourceLabel(new QLabel(this))
    , m_imageIndex(new QSpinBox(this))
    , m_driverList(new QListWidget(this))
    , m_programList(new QListWidget(this))
    , m_confirm(new QCheckBox(QStringLiteral("I understand this will erase the selected disk"), this))
    , m_rebootAfterSuccess(new QCheckBox(QStringLiteral("Reboot after successful install"), this))
    , m_startButton(new QPushButton(QStringLiteral("Start Install"), this))
    , m_refreshDataButton(new QPushButton(QStringLiteral("Refresh Data Source"), this))
    , m_log(new QPlainTextEdit(this))
    , m_process(new QProcess(this))
{
    setObjectName(QStringLiteral("InstallerModule"));

    m_imagePath->setReadOnly(true);
    m_imageIndex->setRange(1, 99);
    m_imageIndex->setValue(1);
    m_log->setReadOnly(true);
    m_startButton->setEnabled(false);
    m_driverList->setSelectionMode(QAbstractItemView::NoSelection);
    m_programList->setSelectionMode(QAbstractItemView::NoSelection);

    auto* diskBrowseButton = new QPushButton(QStringLiteral("Refresh Disks"), this);
    auto* imageBrowse = new QPushButton(QStringLiteral("Browse"), this);

    auto* targetForm = new QFormLayout;
    targetForm->addRow(QStringLiteral("Target disk:"), m_diskCombo);
    targetForm->addRow(QStringLiteral("Windows image:"), m_windowsImagesCombo);
    auto* imageRow = new QHBoxLayout;
    imageRow->addWidget(m_imagePath, 1);
    imageRow->addWidget(imageBrowse);
    targetForm->addRow(QStringLiteral("Image path:"), imageRow);
    targetForm->addRow(QStringLiteral("Image index:"), m_imageIndex);

    auto* imageGroup = new QGroupBox(QStringLiteral("Windows Source"), this);
    auto* imageLayout = new QVBoxLayout(imageGroup);
    imageLayout->addLayout(targetForm);
     imageLayout->addWidget(m_dataSourceLabel);
    imageLayout->addWidget(m_refreshDataButton);

    auto* driverGroup = new QGroupBox(QStringLiteral("Drivers from second partition"), this);
    auto* driverLayout = new QVBoxLayout(driverGroup);
    driverLayout->addWidget(m_driverList, 1);

    auto* programGroup = new QGroupBox(QStringLiteral("Programs to stage for first boot"), this);
    auto* programLayout = new QVBoxLayout(programGroup);
    programLayout->addWidget(m_programList, 1);

    auto* controls = new QHBoxLayout;
    controls->addWidget(diskBrowseButton);
    controls->addStretch(1);
    controls->addWidget(m_confirm);
    controls->addWidget(m_rebootAfterSuccess);
    controls->addWidget(m_startButton);

    auto* sourceLayout = new QHBoxLayout;
    sourceLayout->addWidget(imageGroup, 1);
    sourceLayout->addWidget(driverGroup, 1);
    sourceLayout->addWidget(programGroup, 1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    layout->addLayout(sourceLayout, 1);
    layout->addLayout(controls);
    layout->addWidget(m_log, 2);

    connect(diskBrowseButton, &QPushButton::clicked, this, &InstallerModule::refreshDisks);
    connect(m_refreshDataButton, &QPushButton::clicked, this, &InstallerModule::refreshDataSource);
    connect(imageBrowse, &QPushButton::clicked, this, &InstallerModule::browseImage);
    connect(m_windowsImagesCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &InstallerModule::syncSelectedImage);
    connect(m_confirm, &QCheckBox::toggled, this, &InstallerModule::updateStartState);
    connect(m_imagePath, &QLineEdit::textChanged, this, &InstallerModule::updateStartState);
    connect(m_startButton, &QPushButton::clicked, this, &InstallerModule::startInstall);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &InstallerModule::handleProcessOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &InstallerModule::handleProcessOutput);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &InstallerModule::handleProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &InstallerModule::handleProcessError);

    refreshDisks();
    refreshDataSource();
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

void InstallerModule::refreshDataSource()
{
    setDataSourceLabel(QStringLiteral("Scanning second partition..."));
    enableSelectionWidgets(false);

    PartitionScanner scanner;
    const std::optional<DriveInfo> dataPartition = scanner.findDataPartition();

    if (dataPartition) {
        m_dataPartitionLetter = dataPartition->letter;
        m_dataRootPath = m_dataPartitionLetter + QStringLiteral(":/");
        appendLog(QStringLiteral("Data partition found: %1:\\").arg(m_dataPartitionLetter));
    } else {
        const QString fallbackRoot = PathUtils::dataRoot();
        const QFileInfo rootInfo(fallbackRoot);
        m_dataRootPath = fallbackRoot;
        m_dataPartitionLetter = rootInfo.isRoot() ? rootInfo.absolutePath().left(1) : QString();
        appendLog(QStringLiteral("Data partition not found, using fallback root: %1").arg(fallbackRoot));
    }

    m_dataLoader.reset();
    if (!m_dataPartitionLetter.isEmpty()) {
        m_dataLoader = std::make_unique<DataLoader>(m_dataPartitionLetter);
        m_dataLoader->initialize();
    }

    if (!m_dataLoader || !m_dataLoader->isInitialized()) {
        m_tools.clear();
        m_drivers.clear();
        m_windowsImages.clear();
        m_windowsImagesCombo->clear();
        m_driverList->clear();
        m_programList->clear();
        setDataSourceLabel(QStringLiteral("Second partition not available"));
        updateStartState();
        enableSelectionWidgets(true);
        return;
    }

    m_tools = m_dataLoader->getTools();
    m_drivers = m_dataLoader->getDrivers();
    m_windowsImages = m_dataLoader->getWindowsImages();

    populateWindowsImages();
    populateDriverList();
    populateProgramList();

    QString status = QStringLiteral("Source: %1, images %2, drivers %3, programs %4")
                         .arg(m_dataPartitionLetter.isEmpty() ? m_dataRootPath : m_dataPartitionLetter + QStringLiteral(":"))
                         .arg(m_windowsImages.size())
                         .arg(m_drivers.size())
                         .arg(m_tools.size());
    setDataSourceLabel(status);
    enableSelectionWidgets(true);
    updateStartState();
}

void InstallerModule::populateWindowsImages()
{
    m_windowsImagesCombo->clear();
    for (int i = 0; i < m_windowsImages.size(); ++i) {
        const WindowsImageData& image = m_windowsImages.at(i);
        m_windowsImagesCombo->addItem(windowsImageDescription(image), i);
    }

    if (m_windowsImagesCombo->count() > 0) {
        syncSelectedImage(0);
    } else {
        m_imagePath->clear();
    }
}

void InstallerModule::populateDriverList()
{
    m_driverList->clear();
    for (int i = 0; i < m_drivers.size(); ++i) {
        const DriverData& driver = m_drivers.at(i);
        auto* item = createCheckItem(m_driverList, driver.name, driver.driverPath, i);
        item->setText(QStringLiteral("%1 [%2]").arg(driver.name, categoryLabelForDriver(driver)));
        item->setCheckState(Qt::Checked);
    }
}

void InstallerModule::populateProgramList()
{
    m_programList->clear();
    for (int i = 0; i < m_tools.size(); ++i) {
        const ToolData& tool = m_tools.at(i);
        auto* item = createCheckItem(m_programList, tool.name, tool.executable, i);
        item->setText(QStringLiteral("%1 [%2]").arg(tool.name, tool.category));
        item->setCheckState(Qt::Checked);
    }
}

void InstallerModule::browseImage()
{
    const QString baseDir = m_dataLoader && m_dataLoader->getWindowsImages().isEmpty()
        ? PathUtils::dataRoot()
        : (m_dataPartitionLetter.isEmpty() ? PathUtils::dataRoot() : m_dataPartitionLetter + QStringLiteral(":\\Windows"));

    const QString file = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("Windows image"),
                                                      baseDir,
                                                      QStringLiteral("Windows Images (*.esd *.wim)"));
    if (!file.isEmpty()) {
        m_imagePath->setText(file);
    }
}

void InstallerModule::updateStartState()
{
    const bool ready = m_confirm->isChecked()
        && QFileInfo::exists(m_imagePath->text())
        && m_process->state() == QProcess::NotRunning;
    m_startButton->setEnabled(ready);
}

void InstallerModule::startInstall()
{
    const int diskNumber = m_diskCombo->currentData().toInt();
    const QString imagePath = findSelectedWindowsImagePath();
    if (imagePath.isEmpty()) {
        appendLog(QStringLiteral("No Windows image selected."));
        return;
    }

    if (QMessageBox::warning(this,
                             QStringLiteral("Confirm Install"),
                             QStringLiteral("Disk %1 will be erased and Windows will be applied from:\n%2\n\nSelected drivers: %3\nSelected programs: %4")
                                 .arg(diskNumber)
                                 .arg(QDir::toNativeSeparators(imagePath))
                                 .arg(selectedDrivers().size())
                                 .arg(selectedPrograms().size()),
                             QMessageBox::Cancel | QMessageBox::Yes,
                             QMessageBox::Cancel)
        != QMessageBox::Yes) {
        return;
    }

    m_log->clear();
    m_queue.clear();

    const QVector<DriverData> drivers = selectedDrivers();
    const QVector<ToolData> programs = selectedPrograms();

    m_diskpartScriptPath = writeDiskPartScript(diskNumber);
    if (m_diskpartScriptPath.isEmpty()) {
        appendLog(QStringLiteral("Failed to create DiskPart script."));
        return;
    }

    m_queue.push_back({QStringLiteral("diskpart"), {QStringLiteral("/s"), m_diskpartScriptPath}, QStringLiteral("Partition disk")});
    m_queue.push_back({QStringLiteral("dism"),
                       {QStringLiteral("/Apply-Image"),
                        QStringLiteral("/ImageFile:%1").arg(QDir::toNativeSeparators(imagePath)),
                        QStringLiteral("/Index:%1").arg(m_imageIndex->value()),
                        QStringLiteral("/ApplyDir:W:\\")},
                       QStringLiteral("Apply Windows image")});

    for (const DriverData& driver : drivers) {
        const QString driverPath = QDir::toNativeSeparators(PathUtils::resolveLocalPath(driver.driverPath));
        if (driverPath.isEmpty() || !QFileInfo::exists(driverPath)) {
            continue;
        }

        QStringList arguments;
        arguments << QStringLiteral("/Image:W:\\")
                  << QStringLiteral("/Add-Driver")
                  << QStringLiteral("/Driver:%1").arg(driverPath);

        const QFileInfo driverInfo(driverPath);
        if (driverInfo.isDir()) {
            arguments << QStringLiteral("/Recurse");
        }

        m_queue.push_back({QStringLiteral("dism"), arguments, QStringLiteral("Inject driver %1").arg(driver.name)});
    }

    const QString windowsRootPath = QStringLiteral("W:\\Windows");
    const QVector<ManifestEntry> driverManifest = buildDriverManifest(windowsRootPath);
    const QVector<ManifestEntry> programManifest = buildProgramManifest(windowsRootPath);
    const QVector<ManifestEntry> allManifestEntries = driverManifest + programManifest;

    m_manifestScriptPath = writeManifestFile(allManifestEntries, windowsRootPath);
    m_setupCompleteScriptPath = writeSetupCompleteScript(windowsRootPath);

    if (!m_manifestScriptPath.isEmpty()) {
        const QString copyScript = writeCopyBatch(QStringLiteral("installer manifest"),
                                                  m_manifestScriptPath,
                                                  QStringLiteral("W:\\Windows\\InstallerAgent\\postinstall.json"));
        if (!copyScript.isEmpty()) {
            m_queue.push_back({QStringLiteral("cmd"),
                               {QStringLiteral("/c"), copyScript},
                               QStringLiteral("Write installer manifest")});
        }
    }

    if (!m_setupCompleteScriptPath.isEmpty()) {
        const QString copyScript = writeCopyBatch(QStringLiteral("SetupComplete"),
                                                  m_setupCompleteScriptPath,
                                                  QStringLiteral("W:\\Windows\\Setup\\Scripts\\SetupComplete.cmd"));
        if (!copyScript.isEmpty()) {
            m_queue.push_back({QStringLiteral("cmd"),
                               {QStringLiteral("/c"), copyScript},
                               QStringLiteral("Write SetupComplete.cmd")});
        }
    }

    const QString installerAgent = findInstallerAgentExecutable();
    if (!installerAgent.isEmpty()) {
        const QString copyScript = writeCopyBatch(QStringLiteral("InstallerAgent"),
                                                  QFileInfo(installerAgent).absolutePath(),
                                                  QStringLiteral("W:\\Windows\\InstallerAgent\\"));
        if (!copyScript.isEmpty()) {
            m_queue.push_back({QStringLiteral("cmd"),
                               {QStringLiteral("/c"), copyScript},
                               QStringLiteral("Copy InstallerAgent")});
        }
    }

    m_queue.push_back({QStringLiteral("bcdboot"),
                       {QStringLiteral("W:\\Windows"), QStringLiteral("/s"), QStringLiteral("S:"), QStringLiteral("/f"), QStringLiteral("UEFI")},
                       QStringLiteral("Create bootloader")});

    if (m_rebootAfterSuccess->isChecked()) {
        m_queue.push_back({QStringLiteral("wpeutil"), {QStringLiteral("reboot")}, QStringLiteral("Reboot")});
    }

    m_startButton->setEnabled(false);
    runNextCommand();
}

void InstallerModule::syncSelectedImage(int index)
{
    if (index < 0 || index >= m_windowsImages.size()) {
        return;
    }

    const WindowsImageData& image = m_windowsImages.at(index);
    m_imagePath->setText(image.filePath);
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
        appendLog(QStringLiteral("Command failed. exit=%1 status=%2").arg(exitCode).arg(static_cast<int>(status)));
        m_queue.clear();
        updateStartState();
        return;
    }

    appendLog(QStringLiteral("Command completed."));
    runNextCommand();
}

void InstallerModule::handleProcessError(QProcess::ProcessError error)
{
    appendLog(QStringLiteral("Process error %1: %2").arg(static_cast<int>(error)).arg(m_process->errorString()));
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

void InstallerModule::setDataSourceLabel(const QString& text)
{
    if (m_dataSourceLabel) {
        m_dataSourceLabel->setText(text);
    }
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
    stream << "create partition msr size=16\r\n";
    stream << "create partition primary\r\n";
    stream << "format quick fs=ntfs label=Windows\r\n";
    stream << "assign letter=W\r\n";
    stream << "exit\r\n";
    file.close();
    return file.fileName();
}

QString InstallerModule::writeManifestFile(const QVector<ManifestEntry>& entries, const QString& windowsRootPath)
{
    QTemporaryFile file(QDir::tempPath() + QStringLiteral("/phoenix_postinstall_XXXXXX.json"));
    file.setAutoRemove(false);
    if (!file.open()) {
        return {};
    }

    QJsonArray items;
    for (const ManifestEntry& entry : entries) {
        QJsonObject object;
        object.insert(QStringLiteral("name"), entry.name);
        object.insert(QStringLiteral("sourcePath"), entry.sourcePath);
        object.insert(QStringLiteral("targetPath"), entry.targetPath);
        object.insert(QStringLiteral("category"), entry.category);
        object.insert(QStringLiteral("silentArguments"), entry.silentArguments);
        items.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("windowsRootPath"), windowsRootPath);
    root.insert(QStringLiteral("createdBy"), QStringLiteral("PhoenixShell InstallerModule"));
    root.insert(QStringLiteral("entries"), items);

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return file.fileName();
}

QString InstallerModule::writeSetupCompleteScript(const QString& windowsRootPath)
{
    QTemporaryFile file(QDir::tempPath() + QStringLiteral("/phoenix_setupcomplete_XXXXXX.cmd"));
    file.setAutoRemove(false);
    if (!file.open()) {
        return {};
    }

    QTextStream stream(&file);
    stream << "@echo off\r\n";
    stream << "setlocal\r\n";
    stream << "if exist \"%SystemRoot%\\InstallerAgent\\InstallerAgent.exe\" (\r\n";
    stream << "  \"%SystemRoot%\\InstallerAgent\\InstallerAgent.exe\" --manifest \"%SystemRoot%\\InstallerAgent\\postinstall.json\" --source \"" << windowsRootPath << "\"\r\n";
    stream << ")\r\n";
    stream << "exit /b 0\r\n";
    file.close();
    return file.fileName();
}

QString InstallerModule::writeCopyBatch(const QString& title, const QString& source, const QString& destination)
{
    Q_UNUSED(title)
    QTemporaryFile file(QDir::tempPath() + QStringLiteral("/phoenix_copy_XXXXXX.cmd"));
    file.setAutoRemove(false);
    if (!file.open()) {
        return {};
    }

    QTextStream stream(&file);
    stream << "@echo off\r\n";
    const QFileInfo sourceInfo(source);
    if (sourceInfo.isDir()) {
        stream << "if not exist \"" << destination << "\" mkdir \"" << destination << "\"\r\n";
        stream << "xcopy /E /I /Y \"" << source << "\" \"" << destination << "\"\r\n";
    } else {
        const QString parent = QFileInfo(destination).absolutePath();
        stream << "if not exist \"" << parent << "\" mkdir \"" << parent << "\"\r\n";
        stream << "copy /Y \"" << source << "\" \"" << destination << "\"\r\n";
    }
    file.close();
    return file.fileName();
}

QString InstallerModule::findSelectedWindowsImagePath() const
{
    const QString imagePath = m_imagePath->text().trimmed();
    if (!imagePath.isEmpty()) {
        return PathUtils::resolveLocalPath(imagePath);
    }

    if (m_windowsImagesCombo->currentIndex() >= 0 && m_windowsImagesCombo->currentIndex() < m_windowsImages.size()) {
        return m_windowsImages.at(m_windowsImagesCombo->currentIndex()).filePath;
    }

    return {};
}

QString InstallerModule::findInstallerAgentExecutable() const
{
    const QString candidate = PathUtils::resolveLocalPath(QStringLiteral("InstallerAgent/InstallerAgent.exe"));
    if (QFileInfo::exists(candidate)) {
        return candidate;
    }

    const QString altCandidate = PathUtils::resolveLocalPath(QStringLiteral("InstallerAgent.exe"));
    if (QFileInfo::exists(altCandidate)) {
        return altCandidate;
    }

    return {};
}

QVector<DriverData> InstallerModule::selectedDrivers() const
{
    QVector<DriverData> result;
    for (int i = 0; i < m_driverList->count() && i < m_drivers.size(); ++i) {
        QListWidgetItem* item = m_driverList->item(i);
        if (item && item->checkState() == Qt::Checked) {
            result.push_back(m_drivers.at(i));
        }
    }
    return result;
}

QVector<ToolData> InstallerModule::selectedPrograms() const
{
    QVector<ToolData> result;
    for (int i = 0; i < m_programList->count() && i < m_tools.size(); ++i) {
        QListWidgetItem* item = m_programList->item(i);
        if (item && item->checkState() == Qt::Checked) {
            result.push_back(m_tools.at(i));
        }
    }
    return result;
}

QVector<InstallerModule::ManifestEntry> InstallerModule::buildProgramManifest(const QString& windowsRootPath) const
{
    QVector<ManifestEntry> entries;
    for (const ToolData& tool : selectedPrograms()) {
        ManifestEntry entry;
        entry.name = tool.name;
        entry.sourcePath = PathUtils::resolveLocalPath(tool.workingDir);
        entry.targetPath = windowsRootPath + QStringLiteral("\\InstallerAgent\\Payloads\\Programs\\") + tool.name;
        entry.category = tool.category;
        entry.silentArguments = tool.silentArguments.isEmpty() ? tool.arguments : tool.silentArguments;
        entries.push_back(entry);
    }

    return entries;
}

QVector<InstallerModule::ManifestEntry> InstallerModule::buildDriverManifest(const QString& windowsRootPath) const
{
    QVector<ManifestEntry> entries;
    for (const DriverData& driver : selectedDrivers()) {
        ManifestEntry entry;
        entry.name = driver.name;
        entry.sourcePath = PathUtils::resolveLocalPath(driver.driverPath);
        entry.targetPath = windowsRootPath + QStringLiteral("\\InstallerAgent\\Payloads\\Drivers\\") + driver.name;
        entry.category = driver.category;
        entries.push_back(entry);
    }

    return entries;
}

QListWidgetItem* InstallerModule::createCheckItem(QListWidget* parent, const QString& text, const QString& toolTip, int index)
{
    auto* item = new QListWidgetItem(text, parent);
    item->setToolTip(toolTip);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setData(Qt::UserRole, index);
    item->setCheckState(Qt::Checked);
    return item;
}

void InstallerModule::enableSelectionWidgets(bool enabled)
{
    m_windowsImagesCombo->setEnabled(enabled);
    m_driverList->setEnabled(enabled);
    m_programList->setEnabled(enabled);
    m_confirm->setEnabled(enabled);
    m_rebootAfterSuccess->setEnabled(enabled);
    m_imageIndex->setEnabled(enabled);
}

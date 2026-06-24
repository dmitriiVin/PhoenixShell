#include "SettingsPage.h"

#include "PathUtils.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsPage::SettingsPage(QWidget* parent)
    : QWidget(parent)
    , m_theme(new QComboBox(this))
    , m_wallpapers(new QListWidget(this))
    , m_panelHeight(new QSpinBox(this))
{
    m_theme->addItems({QStringLiteral("Dark"), QStringLiteral("Light"), QStringLiteral("Custom")});
    m_panelHeight->setRange(36, 72);
    m_panelHeight->setValue(44);

    auto* browseButton = new QPushButton(QStringLiteral("Browse"), this);
    auto* wallpaperRow = new QHBoxLayout;
    wallpaperRow->addWidget(m_wallpapers, 1);
    wallpaperRow->addWidget(browseButton);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Theme:"), m_theme);
    form->addRow(QStringLiteral("Taskbar height:"), m_panelHeight);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->addLayout(form);
    layout->addLayout(wallpaperRow, 1);

    connect(m_theme, &QComboBox::currentTextChanged, this, &SettingsPage::themeChanged);
    connect(m_panelHeight, qOverload<int>(&QSpinBox::valueChanged), this, &SettingsPage::panelHeightChanged);
    connect(browseButton, &QPushButton::clicked, this, &SettingsPage::browseWallpaper);
    connect(m_wallpapers, &QListWidget::itemDoubleClicked, this, &SettingsPage::emitSelectedWallpaper);

    loadWallpapers();
}

void SettingsPage::browseWallpaper()
{
    const QString file = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("Wallpaper"),
                                                      PathUtils::dataRoot(),
                                                      QStringLiteral("Images (*.jpg *.jpeg *.png *.bmp)"));
    if (!file.isEmpty()) {
        emit wallpaperChanged(file);
    }
}

void SettingsPage::emitSelectedWallpaper()
{
    if (!m_wallpapers->currentItem()) {
        return;
    }

    emit wallpaperChanged(m_wallpapers->currentItem()->data(Qt::UserRole).toString());
}

void SettingsPage::loadWallpapers()
{
    const QStringList roots = {
        QDir(PathUtils::dataRoot()).filePath(QStringLiteral("Wallpapers")),
        QDir(PathUtils::executableDir()).filePath(QStringLiteral("Wallpapers")),
        QDir(PathUtils::executableDir()).filePath(QStringLiteral("wallpapers"))
    };
    const QStringList filters = {
        QStringLiteral("*.jpg"),
        QStringLiteral("*.jpeg"),
        QStringLiteral("*.png"),
        QStringLiteral("*.bmp")
    };

    for (const QString& root : roots) {
        QDir dir(root);
        if (!dir.exists()) {
            continue;
        }

        const QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);
        for (const QFileInfo& file : files) {
            auto* item = new QListWidgetItem(file.fileName(), m_wallpapers);
            item->setData(Qt::UserRole, file.absoluteFilePath());
        }
    }
}

#pragma once

#include <QWidget>

class QComboBox;
class QListWidget;
class QSpinBox;

class SettingsPage final : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(QWidget* parent = nullptr);

signals:
    void themeChanged(const QString& theme);
    void wallpaperChanged(const QString& path);
    void panelHeightChanged(int height);

private slots:
    void browseWallpaper();
    void emitSelectedWallpaper();

private:
    void loadWallpapers();

    QComboBox* m_theme = nullptr;
    QListWidget* m_wallpapers = nullptr;
    QSpinBox* m_panelHeight = nullptr;
};

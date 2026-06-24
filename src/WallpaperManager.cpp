#include "WallpaperManager.h"

#include "PathUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QLinearGradient>
#include <QPainter>

WallpaperManager::WallpaperManager()
{
    setWallpaper(findDefaultWallpaper());
}

QString WallpaperManager::currentWallpaper() const
{
    return m_currentWallpaper;
}

void WallpaperManager::setWallpaper(const QString& path)
{
    const QString resolved = PathUtils::resolveLocalPath(path);
    QPixmap pixmap(resolved);
    if (pixmap.isNull()) {
        m_currentWallpaper.clear();
        m_pixmap = {};
        return;
    }

    m_currentWallpaper = resolved;
    m_pixmap = pixmap;
}

QString WallpaperManager::findDefaultWallpaper() const
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
        if (!files.isEmpty()) {
            return files.first().absoluteFilePath();
        }
    }

    return {};
}

void WallpaperManager::paint(QPainter* painter, const QRect& rect) const
{
    if (!m_pixmap.isNull()) {
        const QPixmap scaled = m_pixmap.scaled(rect.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const QPoint topLeft(rect.center().x() - scaled.width() / 2,
                             rect.center().y() - scaled.height() / 2);
        painter->drawPixmap(topLeft, scaled);
        return;
    }

    QLinearGradient background(rect.topLeft(), rect.bottomRight());
    background.setColorAt(0.0, QColor(28, 34, 42));
    background.setColorAt(0.45, QColor(38, 55, 60));
    background.setColorAt(1.0, QColor(30, 30, 34));
    painter->fillRect(rect, background);
}

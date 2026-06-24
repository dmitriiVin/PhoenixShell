#pragma once

#include <QPixmap>
#include <QString>

class QPainter;
class QRect;

class WallpaperManager final {
public:
    WallpaperManager();

    QString currentWallpaper() const;
    void setWallpaper(const QString& path);
    QString findDefaultWallpaper() const;
    void paint(QPainter* painter, const QRect& rect) const;

private:
    QString m_currentWallpaper;
    QPixmap m_pixmap;
};

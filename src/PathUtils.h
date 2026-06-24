#pragma once

#include <QString>

namespace PathUtils {

QString executableDir();
QString dataRoot();
QString configPath();
QString resolveLocalPath(const QString& path);
bool isAllowedLocalPath(const QString& path);
bool isAllowedExecutable(const QString& path);
QString displayPath(const QString& path);

}

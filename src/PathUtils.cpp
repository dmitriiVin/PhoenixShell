#include "PathUtils.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace {

QString canonicalOrAbsolute(const QString& path)
{
    QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (!canonical.isEmpty()) {
        return QDir::cleanPath(canonical);
    }

    return QDir::cleanPath(info.absoluteFilePath());
}

bool startsWithPath(const QString& path, const QString& root)
{
    const QString cleanPath = QDir::cleanPath(path).toCaseFolded();
    QString cleanRoot = QDir::cleanPath(root).toCaseFolded();
    if (!cleanRoot.endsWith(QLatin1Char('/'))) {
        cleanRoot += QLatin1Char('/');
    }

    return cleanPath == cleanRoot.left(cleanRoot.size() - 1) || cleanPath.startsWith(cleanRoot);
}

bool looksLikeDataRoot(const QString& path)
{
    QDir dir(path);
    return dir.exists(QStringLiteral("Tools"))
        || dir.exists(QStringLiteral("Windows"))
        || dir.exists(QStringLiteral("Drivers"))
        || dir.exists(QStringLiteral("PhoenixShell"));
}

} // namespace

namespace PathUtils {

QString executableDir()
{
    return QDir::cleanPath(QCoreApplication::applicationDirPath());
}

QString dataRoot()
{
    const QString envRoot = qEnvironmentVariable("PHOENIX_DATA_ROOT");
    if (!envRoot.isEmpty()) {
        return canonicalOrAbsolute(envRoot);
    }

    const QDir appDir(executableDir());
    const QFileInfo appInfo(appDir.absolutePath());
    const QString parent = appInfo.absolutePath();

    const QStringList candidates = {
        appDir.absolutePath(),
        appDir.filePath(QStringLiteral("DATA")),
        QDir(parent).filePath(QStringLiteral("DATA")),
        parent,
        QDir(appInfo.dir().absolutePath()).filePath(QStringLiteral("DATA"))
    };

    for (const QString& candidate : candidates) {
        if (looksLikeDataRoot(candidate)) {
            return canonicalOrAbsolute(candidate);
        }
    }

    return appDir.absolutePath();
}

QString configPath()
{
    const QString appConfig = QDir(executableDir()).filePath(QStringLiteral("config/tools.json"));
    if (QFileInfo::exists(appConfig)) {
        return canonicalOrAbsolute(appConfig);
    }

    const QString dataConfig = QDir(dataRoot()).filePath(QStringLiteral("PhoenixShell/config/tools.json"));
    if (QFileInfo::exists(dataConfig)) {
        return canonicalOrAbsolute(dataConfig);
    }

    return appConfig;
}

QString resolveLocalPath(const QString& path)
{
    if (path.trimmed().isEmpty()) {
        return {};
    }

    QFileInfo info(path);
    if (info.isAbsolute()) {
        return canonicalOrAbsolute(path);
    }

    const QString clean = QDir::cleanPath(path);
    const QStringList candidates = {
        QDir(dataRoot()).filePath(clean),
        QDir(executableDir()).filePath(clean),
        QDir(executableDir()).filePath(QStringLiteral("../") + clean)
    };

    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return canonicalOrAbsolute(candidate);
        }
    }

    return QDir::cleanPath(QDir(dataRoot()).filePath(clean));
}

bool isAllowedLocalPath(const QString& path)
{
    if (path.contains(QStringLiteral("://")) || path.startsWith(QStringLiteral("\\\\"))) {
        return false;
    }

    const QString resolved = canonicalOrAbsolute(resolveLocalPath(path));
    return startsWithPath(resolved, executableDir()) || startsWithPath(resolved, dataRoot());
}

bool isAllowedExecutable(const QString& path)
{
    const QString resolved = resolveLocalPath(path);
    const QFileInfo info(resolved);
    return info.exists()
        && info.isFile()
        && info.suffix().compare(QStringLiteral("exe"), Qt::CaseInsensitive) == 0
        && isAllowedLocalPath(resolved);
}

QString displayPath(const QString& path)
{
    return QDir::toNativeSeparators(resolveLocalPath(path));
}

} // namespace PathUtils

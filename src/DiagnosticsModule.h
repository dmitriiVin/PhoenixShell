#pragma once

#include <QWidget>

class QTextBrowser;

class DiagnosticsModule final : public QWidget {
    Q_OBJECT

public:
    explicit DiagnosticsModule(QWidget* parent = nullptr);

private slots:
    void refresh();

private:
    QString collectSystemInfo() const;
    QString collectDiskInfo() const;
    QString collectNetworkInfo() const;
    QString collectGpuInfo() const;
    QString collectSmartInfo() const;

    QTextBrowser* m_output = nullptr;
};

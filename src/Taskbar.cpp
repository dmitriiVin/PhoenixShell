#include "Taskbar.h"

#include "ShellWindow.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QLabel>
#include <QAbstractSocket>
#include <QNetworkInterface>
#include <QPushButton>
#include <QTimer>

Taskbar::Taskbar(QWidget* parent)
    : QWidget(parent)
    , m_startButton(new QPushButton(QStringLiteral("Start"), this))
    , m_taskArea(new QWidget(this))
    , m_taskLayout(new QHBoxLayout(m_taskArea))
    , m_networkLabel(new QLabel(this))
    , m_diskLabel(new QLabel(this))
    , m_clockLabel(new QLabel(this))
    , m_timer(new QTimer(this))
{
    setObjectName(QStringLiteral("Taskbar"));
    setFixedHeight(44);

    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(8, 5, 8, 5);
    rootLayout->setSpacing(8);

    m_startButton->setObjectName(QStringLiteral("StartButton"));
    m_startButton->setMinimumWidth(88);
    rootLayout->addWidget(m_startButton);

    for (int i = 0; i < 3; ++i) {
        m_workspaceButtons[i] = new QPushButton(QStringLiteral("Desktop %1").arg(i + 1), this);
        m_workspaceButtons[i]->setCheckable(true);
        m_workspaceButtons[i]->setFixedWidth(92);
        rootLayout->addWidget(m_workspaceButtons[i]);
        connect(m_workspaceButtons[i], &QPushButton::clicked, this, [this, i]() {
            emit workspaceRequested(i);
        });
    }

    m_workspaceButtons[0]->setChecked(true);

    m_taskLayout->setContentsMargins(0, 0, 0, 0);
    m_taskLayout->setSpacing(6);
    m_taskLayout->addStretch(1);
    rootLayout->addWidget(m_taskArea, 1);
    rootLayout->addWidget(m_networkLabel);
    rootLayout->addWidget(m_diskLabel);
    rootLayout->addWidget(m_clockLabel);

    connect(m_startButton, &QPushButton::clicked, this, &Taskbar::startRequested);
    connect(m_timer, &QTimer::timeout, this, &Taskbar::refreshStatus);
    m_timer->start(1000);
    refreshStatus();
}

void Taskbar::trackWindow(ShellWindow* window, const QString& title)
{
    if (!window || m_windowButtons.contains(window)) {
        return;
    }

    auto* button = new QPushButton(title, m_taskArea);
    button->setMinimumWidth(120);
    button->setMaximumWidth(220);
    m_taskLayout->insertWidget(qMax(0, m_taskLayout->count() - 1), button);
    m_windowButtons.insert(window, button);

    connect(button, &QPushButton::clicked, this, [this, window]() {
        emit windowButtonRequested(window);
    });
}

void Taskbar::removeWindow(ShellWindow* window)
{
    QPushButton* button = m_windowButtons.take(window);
    if (!button) {
        return;
    }

    button->deleteLater();
}

void Taskbar::setCurrentWorkspace(int workspace)
{
    for (int i = 0; i < 3; ++i) {
        m_workspaceButtons[i]->setChecked(i == workspace);
    }
}

void Taskbar::refreshStatus()
{
    m_networkLabel->setText(networkSummary());
    m_diskLabel->setText(diskSummary());
    m_clockLabel->setText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm")));
}

QString Taskbar::networkSummary() const
{
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        const bool usable = iface.flags().testFlag(QNetworkInterface::IsUp)
            && iface.flags().testFlag(QNetworkInterface::IsRunning)
            && !iface.flags().testFlag(QNetworkInterface::IsLoopBack);
        if (!usable) {
            continue;
        }

        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                return QStringLiteral("NET %1").arg(entry.ip().toString());
            }
        }
    }

    return QStringLiteral("NET offline");
}

QString Taskbar::diskSummary() const
{
    QStringList drives;
    for (const QFileInfo& drive : QDir::drives()) {
        drives << QDir::toNativeSeparators(drive.absoluteFilePath()).left(2);
    }

    return QStringLiteral("DISK %1").arg(drives.join(QLatin1Char(' ')));
}

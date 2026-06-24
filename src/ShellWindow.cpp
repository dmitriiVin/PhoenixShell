#include "ShellWindow.h"

#include <QCloseEvent>
#include <QCursor>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

ShellWindow::ShellWindow(const QString& title, QWidget* content, QWidget* parent)
    : QFrame(parent)
    , m_titleBar(new QWidget(this))
    , m_titleLabel(new QLabel(title, m_titleBar))
    , m_minimizeButton(new QPushButton(m_titleBar))
    , m_maximizeButton(new QPushButton(m_titleBar))
    , m_closeButton(new QPushButton(m_titleBar))
    , m_content(content)
{
    setObjectName(QStringLiteral("ShellWindow"));
    setFrameShape(QFrame::NoFrame);
    setMinimumSize(360, 240);
    setMouseTracking(true);
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::StrongFocus);

    m_titleBar->setObjectName(QStringLiteral("ShellWindowTitleBar"));
    m_titleBar->setFixedHeight(34);
    m_titleBar->installEventFilter(this);
    m_titleBar->setMouseTracking(true);

    m_titleLabel->setObjectName(QStringLiteral("ShellWindowTitle"));
    m_titleLabel->setText(title);

    auto configureButton = [](QPushButton* button, const QString& text) {
        button->setText(text);
        button->setFixedSize(30, 26);
        button->setFocusPolicy(Qt::NoFocus);
    };

    configureButton(m_minimizeButton, QStringLiteral("_"));
    configureButton(m_maximizeButton, QStringLiteral("[]"));
    configureButton(m_closeButton, QStringLiteral("X"));
    m_closeButton->setObjectName(QStringLiteral("ShellWindowCloseButton"));

    auto* titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(10, 4, 4, 4);
    titleLayout->setSpacing(4);
    titleLayout->addWidget(m_titleLabel, 1);
    titleLayout->addWidget(m_minimizeButton);
    titleLayout->addWidget(m_maximizeButton);
    titleLayout->addWidget(m_closeButton);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(6, 6, 6, 6);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(m_titleBar);
    rootLayout->addWidget(content, 1);

    connect(m_minimizeButton, &QPushButton::clicked, this, &ShellWindow::minimizeWindow);
    connect(m_maximizeButton, &QPushButton::clicked, this, &ShellWindow::toggleMaximized);
    connect(m_closeButton, &QPushButton::clicked, this, &ShellWindow::close);
}

QString ShellWindow::title() const
{
    return m_titleLabel->text();
}

bool ShellWindow::isMinimized() const
{
    return m_minimized;
}

void ShellWindow::restoreWindow()
{
    if (m_minimized) {
        m_minimized = false;
        show();
        emit restored(this);
    }

    raise();
    setFocus(Qt::ActiveWindowFocusReason);
    emit activated(this);
}

void ShellWindow::minimizeWindow()
{
    if (m_minimized) {
        return;
    }

    m_minimized = true;
    hide();
    emit minimized(this);
}

void ShellWindow::toggleMaximized()
{
    if (!parentWidget()) {
        return;
    }

    if (!m_maximized) {
        m_normalGeometry = geometry();
        setGeometry(parentWidget()->rect().adjusted(8, 8, -8, -8));
        m_maximized = true;
    } else {
        setGeometry(boundedGeometry(m_normalGeometry));
        m_maximized = false;
    }

    raise();
}

bool ShellWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_titleBar) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton) {
                beginActivate();
                m_dragging = true;
                m_pressGlobal = mouse->globalPosition().toPoint();
                m_pressGeometry = geometry();
                return true;
            }
        }

        if (event->type() == QEvent::MouseMove && m_dragging && !m_maximized) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            const QPoint delta = mouse->globalPosition().toPoint() - m_pressGlobal;
            setGeometry(boundedGeometry(m_pressGeometry.translated(delta)));
            return true;
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            m_dragging = false;
            return true;
        }

        if (event->type() == QEvent::MouseButtonDblClick) {
            toggleMaximized();
            return true;
        }
    }

    return QFrame::eventFilter(watched, event);
}

void ShellWindow::mousePressEvent(QMouseEvent* event)
{
    beginActivate();
    m_resizeEdges = hitTest(event->position().toPoint());
    if (event->button() == Qt::LeftButton && m_resizeEdges != NoEdge && !m_maximized) {
        m_resizing = true;
        m_pressGlobal = event->globalPosition().toPoint();
        m_pressGeometry = geometry();
        event->accept();
        return;
    }

    QFrame::mousePressEvent(event);
}

void ShellWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (m_resizing) {
        const QPoint delta = event->globalPosition().toPoint() - m_pressGlobal;
        QRect next = m_pressGeometry;
        if (m_resizeEdges & Left) {
            next.setLeft(next.left() + delta.x());
        }
        if (m_resizeEdges & Right) {
            next.setRight(next.right() + delta.x());
        }
        if (m_resizeEdges & Top) {
            next.setTop(next.top() + delta.y());
        }
        if (m_resizeEdges & Bottom) {
            next.setBottom(next.bottom() + delta.y());
        }

        if (next.width() < minimumWidth()) {
            if (m_resizeEdges & Left) {
                next.setLeft(next.right() - minimumWidth());
            } else {
                next.setRight(next.left() + minimumWidth());
            }
        }
        if (next.height() < minimumHeight()) {
            if (m_resizeEdges & Top) {
                next.setTop(next.bottom() - minimumHeight());
            } else {
                next.setBottom(next.top() + minimumHeight());
            }
        }

        setGeometry(boundedGeometry(next));
        event->accept();
        return;
    }

    updateCursor(event->position().toPoint());
    QFrame::mouseMoveEvent(event);
}

void ShellWindow::mouseReleaseEvent(QMouseEvent* event)
{
    m_resizing = false;
    m_dragging = false;
    m_resizeEdges = NoEdge;
    updateCursor(event->position().toPoint());
    QFrame::mouseReleaseEvent(event);
}

void ShellWindow::closeEvent(QCloseEvent* event)
{
    emit closed(this);
    QFrame::closeEvent(event);
}

int ShellWindow::hitTest(const QPoint& pos) const
{
    constexpr int border = 7;
    int edges = NoEdge;

    if (pos.x() <= border) {
        edges |= Left;
    } else if (pos.x() >= width() - border) {
        edges |= Right;
    }

    if (pos.y() <= border) {
        edges |= Top;
    } else if (pos.y() >= height() - border) {
        edges |= Bottom;
    }

    return edges;
}

void ShellWindow::updateCursor(const QPoint& pos)
{
    if (m_maximized) {
        unsetCursor();
        return;
    }

    const int edges = hitTest(pos);
    if ((edges & Left && edges & Top) || (edges & Right && edges & Bottom)) {
        setCursor(Qt::SizeFDiagCursor);
    } else if ((edges & Right && edges & Top) || (edges & Left && edges & Bottom)) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (edges & Left || edges & Right) {
        setCursor(Qt::SizeHorCursor);
    } else if (edges & Top || edges & Bottom) {
        setCursor(Qt::SizeVerCursor);
    } else {
        unsetCursor();
    }
}

void ShellWindow::beginActivate()
{
    raise();
    setFocus(Qt::MouseFocusReason);
    emit activated(this);
}

QRect ShellWindow::boundedGeometry(const QRect& geometry) const
{
    if (!parentWidget()) {
        return geometry;
    }

    QRect bounds = parentWidget()->rect().adjusted(0, 0, -1, -1);
    QRect next = geometry;
    if (next.width() > bounds.width()) {
        next.setWidth(bounds.width());
    }
    if (next.height() > bounds.height()) {
        next.setHeight(bounds.height());
    }

    if (next.left() < bounds.left()) {
        next.moveLeft(bounds.left());
    }
    if (next.top() < bounds.top()) {
        next.moveTop(bounds.top());
    }
    if (next.right() > bounds.right()) {
        next.moveRight(bounds.right());
    }
    if (next.bottom() > bounds.bottom()) {
        next.moveBottom(bounds.bottom());
    }

    return next;
}

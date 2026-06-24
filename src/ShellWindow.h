#pragma once

#include <QFrame>
#include <QPoint>
#include <QPointer>
#include <QRect>

class QLabel;
class QPushButton;

class ShellWindow final : public QFrame {
    Q_OBJECT

public:
    explicit ShellWindow(const QString& title, QWidget* content, QWidget* parent = nullptr);

    QString title() const;
    bool isMinimized() const;

public slots:
    void restoreWindow();
    void minimizeWindow();
    void toggleMaximized();

signals:
    void activated(ShellWindow* window);
    void minimized(ShellWindow* window);
    void restored(ShellWindow* window);
    void closed(ShellWindow* window);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    enum ResizeEdge {
        NoEdge = 0,
        Left = 1,
        Right = 2,
        Top = 4,
        Bottom = 8
    };

    int hitTest(const QPoint& pos) const;
    void updateCursor(const QPoint& pos);
    void beginActivate();
    QRect boundedGeometry(const QRect& geometry) const;

    QWidget* m_titleBar = nullptr;
    QLabel* m_titleLabel = nullptr;
    QPushButton* m_minimizeButton = nullptr;
    QPushButton* m_maximizeButton = nullptr;
    QPushButton* m_closeButton = nullptr;
    QPointer<QWidget> m_content;

    bool m_dragging = false;
    bool m_resizing = false;
    bool m_minimized = false;
    bool m_maximized = false;
    int m_resizeEdges = NoEdge;
    QPoint m_pressGlobal;
    QRect m_pressGeometry;
    QRect m_normalGeometry;
};

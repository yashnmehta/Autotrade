#ifndef CUSTOMTITLEBAR_H
#define CUSTOMTITLEBAR_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>

class CustomTitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit CustomTitleBar(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    QString title() const;

signals:
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();
    void doubleClicked();
    void dragStarted(const QPoint &globalPos);
    void dragMoved(const QPoint &globalPos);
    void dragEnded();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    QLabel *m_titleLabel;
    QPushButton *m_minimizeButton;
    QPushButton *m_maximizeButton;
    QPushButton *m_closeButton;

    QPoint m_dragPosition;
    bool m_isDragging;
};

#endif // CUSTOMTITLEBAR_H

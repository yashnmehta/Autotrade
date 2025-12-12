#ifndef CUSTOMMDICHILD_H
#define CUSTOMMDICHILD_H

#include <QWidget>
#include <QVBoxLayout>

class CustomTitleBar;

class CustomMDIChild : public QWidget
{
    Q_OBJECT

public:
    explicit CustomMDIChild(const QString &title, QWidget *parent = nullptr);

    void setContentWidget(QWidget *widget);
    QWidget *contentWidget() const;

    void setTitle(const QString &title);
    QString title() const;

signals:
    void closeRequested();
    void minimizeRequested();

private:
    CustomTitleBar *m_titleBar;
    QWidget *m_contentWidget;
    QVBoxLayout *m_mainLayout;
};

#endif // CUSTOMMDICHILD_H

#ifndef MDITASKBAR_H
#define MDITASKBAR_H

#include <QWidget>
#include <QHBoxLayout>
#include <QMap>
#include <QPushButton>

class CustomMDISubWindow;

/**
 * @brief MDI TaskBar - Shows minimized windows (like Windows taskbar)
 * 
 * Pure C++ implementation. Each minimized window gets a button.
 */
class MDITaskBar : public QWidget
{
    Q_OBJECT

public:
    explicit MDITaskBar(QWidget *parent = nullptr);
    
    void addWindow(CustomMDISubWindow *window);
    void removeWindow(CustomMDISubWindow *window);
    void updateWindowTitle(CustomMDISubWindow *window, const QString &title);

signals:
    void windowRestoreRequested(CustomMDISubWindow *window);

private:
    QHBoxLayout *m_layout;
    QMap<CustomMDISubWindow*, QPushButton*> m_windowButtons;
};

#endif // MDITASKBAR_H

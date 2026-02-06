#ifndef SELLWINDOW_H
#define SELLWINDOW_H

#include "views/BaseOrderWindow.h"

class SellWindow : public BaseOrderWindow
{
    Q_OBJECT
public:
    explicit SellWindow(QWidget *parent = nullptr);
    explicit SellWindow(const WindowContext &context, QWidget *parent = nullptr);
    ~SellWindow();

    static SellWindow* getInstance(QWidget *parent = nullptr);
    static SellWindow* getInstance(const WindowContext &context, QWidget *parent = nullptr);
    static void closeCurrentWindow();
    static bool hasActiveWindow();

signals:
    void requestBuyWithContext(const WindowContext &context);

protected:
    void onSubmitClicked() override;
    void calculateDefaultPrice(const WindowContext &context) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private:
    static void setInstance(SellWindow* instance);
    static SellWindow* s_instance;
};

#endif // SELLWINDOW_H

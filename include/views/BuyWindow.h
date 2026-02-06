#ifndef BUYWINDOW_H
#define BUYWINDOW_H

#include "views/BaseOrderWindow.h"

class BuyWindow : public BaseOrderWindow
{
    Q_OBJECT
public:
    explicit BuyWindow(QWidget *parent = nullptr);
    explicit BuyWindow(const WindowContext &context, QWidget *parent = nullptr);
    ~BuyWindow();

    static BuyWindow* getInstance(QWidget *parent = nullptr);
    static BuyWindow* getInstance(const WindowContext &context, QWidget *parent = nullptr);
    static void closeCurrentWindow();
    static bool hasActiveWindow();

signals:
    void requestSellWithContext(const WindowContext &context);

protected:
    void onSubmitClicked() override;
    void calculateDefaultPrice(const WindowContext &context) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private:
    static void setInstance(BuyWindow* instance);
    static BuyWindow* s_instance;
};

#endif // BUYWINDOW_H

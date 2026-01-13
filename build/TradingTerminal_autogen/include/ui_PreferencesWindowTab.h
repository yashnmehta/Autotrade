/********************************************************************************
** Form generated from reading UI file 'PreferencesWindowTab.ui'
**
** Created by: Qt User Interface Compiler version 5.14.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PREFERENCESWINDOWTAB_H
#define UI_PREFERENCESWINDOWTAB_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_PreferencesWindowTab
{
public:
    QVBoxLayout *verticalLayout_main;
    QTabWidget *tabWidget;
    QWidget *tabGeneral;
    QVBoxLayout *verticalLayout_general;
    QWidget *tabOrder;
    QVBoxLayout *verticalLayout_order;
    QWidget *tabPortfolio;
    QVBoxLayout *verticalLayout_portfolio;
    QWidget *tabWorkSpace;
    QVBoxLayout *verticalLayout_workspace;
    QWidget *tabDerivatives;
    QVBoxLayout *verticalLayout_derivatives;
    QWidget *tabAlertsMsg;
    QVBoxLayout *verticalLayout_alertsmsg;
    QWidget *tabMarginPlusOrder;
    QVBoxLayout *verticalLayout_marginplus;
    QHBoxLayout *horizontalLayout_buttons;
    QSpacerItem *horizontalSpacer;
    QPushButton *pushButton_ok;
    QPushButton *pushButton_cancel;
    QPushButton *pushButton_apply;

    void setupUi(QWidget *PreferencesWindowTab)
    {
        if (PreferencesWindowTab->objectName().isEmpty())
            PreferencesWindowTab->setObjectName(QString::fromUtf8("PreferencesWindowTab"));
        PreferencesWindowTab->resize(750, 650);
        verticalLayout_main = new QVBoxLayout(PreferencesWindowTab);
        verticalLayout_main->setSpacing(6);
        verticalLayout_main->setObjectName(QString::fromUtf8("verticalLayout_main"));
        verticalLayout_main->setContentsMargins(10, 10, 10, 10);
        tabWidget = new QTabWidget(PreferencesWindowTab);
        tabWidget->setObjectName(QString::fromUtf8("tabWidget"));
        tabGeneral = new QWidget();
        tabGeneral->setObjectName(QString::fromUtf8("tabGeneral"));
        verticalLayout_general = new QVBoxLayout(tabGeneral);
        verticalLayout_general->setSpacing(0);
        verticalLayout_general->setObjectName(QString::fromUtf8("verticalLayout_general"));
        verticalLayout_general->setContentsMargins(0, 0, 0, 0);
        tabWidget->addTab(tabGeneral, QString());
        tabOrder = new QWidget();
        tabOrder->setObjectName(QString::fromUtf8("tabOrder"));
        verticalLayout_order = new QVBoxLayout(tabOrder);
        verticalLayout_order->setSpacing(0);
        verticalLayout_order->setObjectName(QString::fromUtf8("verticalLayout_order"));
        verticalLayout_order->setContentsMargins(0, 0, 0, 0);
        tabWidget->addTab(tabOrder, QString());
        tabPortfolio = new QWidget();
        tabPortfolio->setObjectName(QString::fromUtf8("tabPortfolio"));
        verticalLayout_portfolio = new QVBoxLayout(tabPortfolio);
        verticalLayout_portfolio->setSpacing(0);
        verticalLayout_portfolio->setObjectName(QString::fromUtf8("verticalLayout_portfolio"));
        verticalLayout_portfolio->setContentsMargins(0, 0, 0, 0);
        tabWidget->addTab(tabPortfolio, QString());
        tabWorkSpace = new QWidget();
        tabWorkSpace->setObjectName(QString::fromUtf8("tabWorkSpace"));
        verticalLayout_workspace = new QVBoxLayout(tabWorkSpace);
        verticalLayout_workspace->setSpacing(0);
        verticalLayout_workspace->setObjectName(QString::fromUtf8("verticalLayout_workspace"));
        verticalLayout_workspace->setContentsMargins(0, 0, 0, 0);
        tabWidget->addTab(tabWorkSpace, QString());
        tabDerivatives = new QWidget();
        tabDerivatives->setObjectName(QString::fromUtf8("tabDerivatives"));
        verticalLayout_derivatives = new QVBoxLayout(tabDerivatives);
        verticalLayout_derivatives->setSpacing(0);
        verticalLayout_derivatives->setObjectName(QString::fromUtf8("verticalLayout_derivatives"));
        verticalLayout_derivatives->setContentsMargins(0, 0, 0, 0);
        tabWidget->addTab(tabDerivatives, QString());
        tabAlertsMsg = new QWidget();
        tabAlertsMsg->setObjectName(QString::fromUtf8("tabAlertsMsg"));
        verticalLayout_alertsmsg = new QVBoxLayout(tabAlertsMsg);
        verticalLayout_alertsmsg->setSpacing(0);
        verticalLayout_alertsmsg->setObjectName(QString::fromUtf8("verticalLayout_alertsmsg"));
        verticalLayout_alertsmsg->setContentsMargins(0, 0, 0, 0);
        tabWidget->addTab(tabAlertsMsg, QString());
        tabMarginPlusOrder = new QWidget();
        tabMarginPlusOrder->setObjectName(QString::fromUtf8("tabMarginPlusOrder"));
        verticalLayout_marginplus = new QVBoxLayout(tabMarginPlusOrder);
        verticalLayout_marginplus->setSpacing(0);
        verticalLayout_marginplus->setObjectName(QString::fromUtf8("verticalLayout_marginplus"));
        verticalLayout_marginplus->setContentsMargins(0, 0, 0, 0);
        tabWidget->addTab(tabMarginPlusOrder, QString());

        verticalLayout_main->addWidget(tabWidget);

        horizontalLayout_buttons = new QHBoxLayout();
        horizontalLayout_buttons->setSpacing(6);
        horizontalLayout_buttons->setObjectName(QString::fromUtf8("horizontalLayout_buttons"));
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_buttons->addItem(horizontalSpacer);

        pushButton_ok = new QPushButton(PreferencesWindowTab);
        pushButton_ok->setObjectName(QString::fromUtf8("pushButton_ok"));
        pushButton_ok->setMinimumSize(QSize(80, 0));

        horizontalLayout_buttons->addWidget(pushButton_ok);

        pushButton_cancel = new QPushButton(PreferencesWindowTab);
        pushButton_cancel->setObjectName(QString::fromUtf8("pushButton_cancel"));
        pushButton_cancel->setMinimumSize(QSize(80, 0));

        horizontalLayout_buttons->addWidget(pushButton_cancel);

        pushButton_apply = new QPushButton(PreferencesWindowTab);
        pushButton_apply->setObjectName(QString::fromUtf8("pushButton_apply"));
        pushButton_apply->setMinimumSize(QSize(80, 0));

        horizontalLayout_buttons->addWidget(pushButton_apply);


        verticalLayout_main->addLayout(horizontalLayout_buttons);


        retranslateUi(PreferencesWindowTab);

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(PreferencesWindowTab);
    } // setupUi

    void retranslateUi(QWidget *PreferencesWindowTab)
    {
        PreferencesWindowTab->setWindowTitle(QCoreApplication::translate("PreferencesWindowTab", "Preferences", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tabGeneral), QCoreApplication::translate("PreferencesWindowTab", "General", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tabOrder), QCoreApplication::translate("PreferencesWindowTab", "Order", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tabPortfolio), QCoreApplication::translate("PreferencesWindowTab", "Portfolio", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tabWorkSpace), QCoreApplication::translate("PreferencesWindowTab", "Work-Space", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tabDerivatives), QCoreApplication::translate("PreferencesWindowTab", "Derivatives", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tabAlertsMsg), QCoreApplication::translate("PreferencesWindowTab", "Alerts Msg", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tabMarginPlusOrder), QCoreApplication::translate("PreferencesWindowTab", "Bracket/Margin Plus Order", nullptr));
        pushButton_ok->setText(QCoreApplication::translate("PreferencesWindowTab", "OK", nullptr));
        pushButton_cancel->setText(QCoreApplication::translate("PreferencesWindowTab", "Cancel", nullptr));
        pushButton_apply->setText(QCoreApplication::translate("PreferencesWindowTab", "Apply", nullptr));
    } // retranslateUi

};

namespace Ui {
    class PreferencesWindowTab: public Ui_PreferencesWindowTab {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PREFERENCESWINDOWTAB_H

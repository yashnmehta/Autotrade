/********************************************************************************
** Form generated from reading UI file 'InfoBar.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_INFOBAR_H
#define UI_INFOBAR_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_InfoBar
{
public:
    QHBoxLayout *horizontalLayout;
    QLabel *versionLabel;
    QLabel *userLabel;
    QWidget *widget;
    QHBoxLayout *segmentLayout;
    QLabel *infoLabel;
    QLabel *lastUpdateLabel;
    QLabel *openOrdersLabel;
    QLabel *totalOrdersLabel;
    QLabel *totalTradesLabel;

    void setupUi(QWidget *InfoBar)
    {
        if (InfoBar->objectName().isEmpty())
            InfoBar->setObjectName(QString::fromUtf8("InfoBar"));
        InfoBar->resize(1200, 50);
        InfoBar->setMinimumSize(QSize(0, 50));
        InfoBar->setStyleSheet(QString::fromUtf8("QWidget{ background-color: #e1e1e1; }\n"
" QLabel { background-color: #e1e1e1; \n"
" font-size: 10px; padding: 0px 0px; }\n"
"QFrame#panel{ background-color: #ffffff }"));
        horizontalLayout = new QHBoxLayout(InfoBar);
        horizontalLayout->setSpacing(0);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        versionLabel = new QLabel(InfoBar);
        versionLabel->setObjectName(QString::fromUtf8("versionLabel"));
        QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(versionLabel->sizePolicy().hasHeightForWidth());
        versionLabel->setSizePolicy(sizePolicy);
        versionLabel->setMinimumSize(QSize(50, 0));

        horizontalLayout->addWidget(versionLabel);

        userLabel = new QLabel(InfoBar);
        userLabel->setObjectName(QString::fromUtf8("userLabel"));
        sizePolicy.setHeightForWidth(userLabel->sizePolicy().hasHeightForWidth());
        userLabel->setSizePolicy(sizePolicy);
        userLabel->setMinimumSize(QSize(80, 0));

        horizontalLayout->addWidget(userLabel);

        widget = new QWidget(InfoBar);
        widget->setObjectName(QString::fromUtf8("widget"));
        segmentLayout = new QHBoxLayout(widget);
        segmentLayout->setSpacing(6);
        segmentLayout->setObjectName(QString::fromUtf8("segmentLayout"));

        horizontalLayout->addWidget(widget);

        infoLabel = new QLabel(InfoBar);
        infoLabel->setObjectName(QString::fromUtf8("infoLabel"));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy1.setHorizontalStretch(1);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(infoLabel->sizePolicy().hasHeightForWidth());
        infoLabel->setSizePolicy(sizePolicy1);
        infoLabel->setAlignment(Qt::AlignCenter);

        horizontalLayout->addWidget(infoLabel);

        lastUpdateLabel = new QLabel(InfoBar);
        lastUpdateLabel->setObjectName(QString::fromUtf8("lastUpdateLabel"));

        horizontalLayout->addWidget(lastUpdateLabel);

        openOrdersLabel = new QLabel(InfoBar);
        openOrdersLabel->setObjectName(QString::fromUtf8("openOrdersLabel"));
        sizePolicy.setHeightForWidth(openOrdersLabel->sizePolicy().hasHeightForWidth());
        openOrdersLabel->setSizePolicy(sizePolicy);
        openOrdersLabel->setMinimumSize(QSize(60, 0));

        horizontalLayout->addWidget(openOrdersLabel);

        totalOrdersLabel = new QLabel(InfoBar);
        totalOrdersLabel->setObjectName(QString::fromUtf8("totalOrdersLabel"));
        sizePolicy.setHeightForWidth(totalOrdersLabel->sizePolicy().hasHeightForWidth());
        totalOrdersLabel->setSizePolicy(sizePolicy);
        totalOrdersLabel->setMinimumSize(QSize(70, 0));

        horizontalLayout->addWidget(totalOrdersLabel);

        totalTradesLabel = new QLabel(InfoBar);
        totalTradesLabel->setObjectName(QString::fromUtf8("totalTradesLabel"));
        sizePolicy.setHeightForWidth(totalTradesLabel->sizePolicy().hasHeightForWidth());
        totalTradesLabel->setSizePolicy(sizePolicy);
        totalTradesLabel->setMinimumSize(QSize(70, 0));

        horizontalLayout->addWidget(totalTradesLabel);


        retranslateUi(InfoBar);

        QMetaObject::connectSlotsByName(InfoBar);
    } // setupUi

    void retranslateUi(QWidget *InfoBar)
    {
        versionLabel->setText(QCoreApplication::translate("InfoBar", "v1.0.0", nullptr));
        userLabel->setText(QCoreApplication::translate("InfoBar", "User: Demo", nullptr));
        infoLabel->setText(QCoreApplication::translate("InfoBar", "Ready", nullptr));
        lastUpdateLabel->setText(QCoreApplication::translate("InfoBar", "Last Update: --", nullptr));
        openOrdersLabel->setText(QCoreApplication::translate("InfoBar", "Open: 0", nullptr));
        totalOrdersLabel->setText(QCoreApplication::translate("InfoBar", "Orders: 0", nullptr));
        totalTradesLabel->setText(QCoreApplication::translate("InfoBar", "Trades: 0", nullptr));
        (void)InfoBar;
    } // retranslateUi

};

namespace Ui {
    class InfoBar: public Ui_InfoBar {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_INFOBAR_H

/********************************************************************************
** Form generated from reading UI file 'SplashScreen.ui'
**
** Created by: Qt User Interface Compiler version 5.14.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SPLASHSCREEN_H
#define UI_SPLASHSCREEN_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_SplashScreen
{
public:
    QVBoxLayout *verticalLayout;
    QSpacerItem *verticalSpacer;
    QLabel *labelTitle;
    QLabel *labelVersion;
    QSpacerItem *verticalSpacer_2;
    QProgressBar *progressBar;
    QLabel *labelStatus;
    QSpacerItem *verticalSpacer_3;

    void setupUi(QWidget *SplashScreen)
    {
        if (SplashScreen->objectName().isEmpty())
            SplashScreen->setObjectName(QString::fromUtf8("SplashScreen"));
        SplashScreen->resize(500, 300);
        SplashScreen->setStyleSheet(QString::fromUtf8("\n"
"QWidget#SplashScreen {\n"
"    background-color: #1e1e1e;\n"
"    border: 2px solid #3d3d3d;\n"
"    border-radius: 10px;\n"
"}\n"
"\n"
"QLabel {\n"
"    color: #ffffff;\n"
"}\n"
"\n"
"QProgressBar {\n"
"    border: 2px solid #3d3d3d;\n"
"    border-radius: 5px;\n"
"    text-align: center;\n"
"    background-color: #2d2d2d;\n"
"    color: #ffffff;\n"
"}\n"
"\n"
"QProgressBar::chunk {\n"
"    background-color: qlineargradient(\n"
"        x1:0, y1:0, x2:1, y2:0,\n"
"        stop:0 #00aaff, stop:1 #0077cc\n"
"    );\n"
"    border-radius: 3px;\n"
"}\n"
"   "));
        verticalLayout = new QVBoxLayout(SplashScreen);
        verticalLayout->setSpacing(20);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(30, 30, 30, 30);
        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);

        labelTitle = new QLabel(SplashScreen);
        labelTitle->setObjectName(QString::fromUtf8("labelTitle"));
        QFont font;
        font.setPointSize(24);
        font.setBold(true);
        font.setWeight(75);
        labelTitle->setFont(font);
        labelTitle->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(labelTitle);

        labelVersion = new QLabel(SplashScreen);
        labelVersion->setObjectName(QString::fromUtf8("labelVersion"));
        QFont font1;
        font1.setPointSize(12);
        labelVersion->setFont(font1);
        labelVersion->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(labelVersion);

        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_2);

        progressBar = new QProgressBar(SplashScreen);
        progressBar->setObjectName(QString::fromUtf8("progressBar"));
        progressBar->setValue(0);
        progressBar->setTextVisible(true);

        verticalLayout->addWidget(progressBar);

        labelStatus = new QLabel(SplashScreen);
        labelStatus->setObjectName(QString::fromUtf8("labelStatus"));
        QFont font2;
        font2.setPointSize(10);
        labelStatus->setFont(font2);
        labelStatus->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(labelStatus);

        verticalSpacer_3 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_3);


        retranslateUi(SplashScreen);

        QMetaObject::connectSlotsByName(SplashScreen);
    } // setupUi

    void retranslateUi(QWidget *SplashScreen)
    {
        SplashScreen->setWindowTitle(QCoreApplication::translate("SplashScreen", "Loading...", nullptr));
        labelTitle->setText(QCoreApplication::translate("SplashScreen", "Trading Terminal", nullptr));
        labelVersion->setText(QCoreApplication::translate("SplashScreen", "Version 1.0.0", nullptr));
        labelStatus->setText(QCoreApplication::translate("SplashScreen", "Initializing...", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SplashScreen: public Ui_SplashScreen {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SPLASHSCREEN_H

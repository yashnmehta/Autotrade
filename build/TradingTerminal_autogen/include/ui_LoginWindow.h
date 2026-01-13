/********************************************************************************
** Form generated from reading UI file 'LoginWindow.ui'
**
** Created by: Qt User Interface Compiler version 5.14.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LOGINWINDOW_H
#define UI_LOGINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_LoginWindow
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *labelTitle;
    QGroupBox *groupBoxMarketData;
    QFormLayout *formLayout;
    QLabel *labelMDAppKey;
    QLineEdit *lb_md_appKey;
    QLabel *labelMDSecretKey;
    QLineEdit *lb_md_secretKey;
    QLabel *lbMDStatus;
    QGroupBox *groupBoxInteractive;
    QFormLayout *formLayout_2;
    QLabel *labelIAAppKey;
    QLineEdit *lb_ia_appKey;
    QLabel *labelIASecretKey;
    QLineEdit *lb_ia_secretKey;
    QLabel *lbIAStatus;
    QGroupBox *groupBoxUser;
    QFormLayout *formLayout_3;
    QLabel *labelLoginID;
    QLineEdit *lb_loginId;
    QLabel *labelClientCode;
    QLineEdit *leClient;
    QCheckBox *cbCmaster;
    QSpacerItem *verticalSpacer;
    QHBoxLayout *horizontalLayout;
    QPushButton *pbLogin;
    QPushButton *pbNext;
    QPushButton *pbExit;

    void setupUi(QDialog *LoginWindow)
    {
        if (LoginWindow->objectName().isEmpty())
            LoginWindow->setObjectName(QString::fromUtf8("LoginWindow"));
        LoginWindow->resize(500, 550);
        LoginWindow->setModal(true);
        LoginWindow->setStyleSheet(QString::fromUtf8("\n"
"QDialog {\n"
"    background-color: #2b2b2b;\n"
"}\n"
"\n"
"QLabel {\n"
"    color: #ffffff;\n"
"}\n"
"\n"
"QLineEdit {\n"
"    background-color: #3d3d3d;\n"
"    border: 1px solid #555555;\n"
"    border-radius: 3px;\n"
"    padding: 5px;\n"
"    color: #ffffff;\n"
"}\n"
"\n"
"QLineEdit:focus {\n"
"    border: 1px solid #00aaff;\n"
"}\n"
"\n"
"QPushButton {\n"
"    background-color: #0078d4;\n"
"    color: white;\n"
"    border: none;\n"
"    border-radius: 3px;\n"
"    padding: 8px 16px;\n"
"    font-weight: bold;\n"
"}\n"
"\n"
"QPushButton:hover {\n"
"    background-color: #005a9e;\n"
"}\n"
"\n"
"QPushButton:pressed {\n"
"    background-color: #004578;\n"
"}\n"
"\n"
"QPushButton:disabled {\n"
"    background-color: #555555;\n"
"    color: #999999;\n"
"}\n"
"\n"
"QCheckBox {\n"
"    color: #ffffff;\n"
"}\n"
"\n"
"QGroupBox {\n"
"    color: #ffffff;\n"
"    border: 1px solid #555555;\n"
"    border-radius: 5px;\n"
"    margin-top: 10px;\n"
"    padding-top: 10px;\n"
"}\n"
"\n"
"QGroupBox::title {\n"
"   "
                        " subcontrol-origin: margin;\n"
"    subcontrol-position: top left;\n"
"    padding: 0 5px;\n"
"}\n"
"   "));
        verticalLayout = new QVBoxLayout(LoginWindow);
        verticalLayout->setSpacing(15);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(20, 20, 20, 20);
        labelTitle = new QLabel(LoginWindow);
        labelTitle->setObjectName(QString::fromUtf8("labelTitle"));
        QFont font;
        font.setPointSize(16);
        font.setBold(true);
        font.setWeight(75);
        labelTitle->setFont(font);
        labelTitle->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(labelTitle);

        groupBoxMarketData = new QGroupBox(LoginWindow);
        groupBoxMarketData->setObjectName(QString::fromUtf8("groupBoxMarketData"));
        formLayout = new QFormLayout(groupBoxMarketData);
        formLayout->setObjectName(QString::fromUtf8("formLayout"));
        labelMDAppKey = new QLabel(groupBoxMarketData);
        labelMDAppKey->setObjectName(QString::fromUtf8("labelMDAppKey"));

        formLayout->setWidget(0, QFormLayout::LabelRole, labelMDAppKey);

        lb_md_appKey = new QLineEdit(groupBoxMarketData);
        lb_md_appKey->setObjectName(QString::fromUtf8("lb_md_appKey"));

        formLayout->setWidget(0, QFormLayout::FieldRole, lb_md_appKey);

        labelMDSecretKey = new QLabel(groupBoxMarketData);
        labelMDSecretKey->setObjectName(QString::fromUtf8("labelMDSecretKey"));

        formLayout->setWidget(1, QFormLayout::LabelRole, labelMDSecretKey);

        lb_md_secretKey = new QLineEdit(groupBoxMarketData);
        lb_md_secretKey->setObjectName(QString::fromUtf8("lb_md_secretKey"));
        lb_md_secretKey->setEchoMode(QLineEdit::Password);

        formLayout->setWidget(1, QFormLayout::FieldRole, lb_md_secretKey);

        lbMDStatus = new QLabel(groupBoxMarketData);
        lbMDStatus->setObjectName(QString::fromUtf8("lbMDStatus"));
        lbMDStatus->setAlignment(Qt::AlignCenter);

        formLayout->setWidget(2, QFormLayout::SpanningRole, lbMDStatus);


        verticalLayout->addWidget(groupBoxMarketData);

        groupBoxInteractive = new QGroupBox(LoginWindow);
        groupBoxInteractive->setObjectName(QString::fromUtf8("groupBoxInteractive"));
        formLayout_2 = new QFormLayout(groupBoxInteractive);
        formLayout_2->setObjectName(QString::fromUtf8("formLayout_2"));
        labelIAAppKey = new QLabel(groupBoxInteractive);
        labelIAAppKey->setObjectName(QString::fromUtf8("labelIAAppKey"));

        formLayout_2->setWidget(0, QFormLayout::LabelRole, labelIAAppKey);

        lb_ia_appKey = new QLineEdit(groupBoxInteractive);
        lb_ia_appKey->setObjectName(QString::fromUtf8("lb_ia_appKey"));

        formLayout_2->setWidget(0, QFormLayout::FieldRole, lb_ia_appKey);

        labelIASecretKey = new QLabel(groupBoxInteractive);
        labelIASecretKey->setObjectName(QString::fromUtf8("labelIASecretKey"));

        formLayout_2->setWidget(1, QFormLayout::LabelRole, labelIASecretKey);

        lb_ia_secretKey = new QLineEdit(groupBoxInteractive);
        lb_ia_secretKey->setObjectName(QString::fromUtf8("lb_ia_secretKey"));
        lb_ia_secretKey->setEchoMode(QLineEdit::Password);

        formLayout_2->setWidget(1, QFormLayout::FieldRole, lb_ia_secretKey);

        lbIAStatus = new QLabel(groupBoxInteractive);
        lbIAStatus->setObjectName(QString::fromUtf8("lbIAStatus"));
        lbIAStatus->setAlignment(Qt::AlignCenter);

        formLayout_2->setWidget(2, QFormLayout::SpanningRole, lbIAStatus);


        verticalLayout->addWidget(groupBoxInteractive);

        groupBoxUser = new QGroupBox(LoginWindow);
        groupBoxUser->setObjectName(QString::fromUtf8("groupBoxUser"));
        formLayout_3 = new QFormLayout(groupBoxUser);
        formLayout_3->setObjectName(QString::fromUtf8("formLayout_3"));
        labelLoginID = new QLabel(groupBoxUser);
        labelLoginID->setObjectName(QString::fromUtf8("labelLoginID"));

        formLayout_3->setWidget(0, QFormLayout::LabelRole, labelLoginID);

        lb_loginId = new QLineEdit(groupBoxUser);
        lb_loginId->setObjectName(QString::fromUtf8("lb_loginId"));

        formLayout_3->setWidget(0, QFormLayout::FieldRole, lb_loginId);

        labelClientCode = new QLabel(groupBoxUser);
        labelClientCode->setObjectName(QString::fromUtf8("labelClientCode"));

        formLayout_3->setWidget(1, QFormLayout::LabelRole, labelClientCode);

        leClient = new QLineEdit(groupBoxUser);
        leClient->setObjectName(QString::fromUtf8("leClient"));

        formLayout_3->setWidget(1, QFormLayout::FieldRole, leClient);


        verticalLayout->addWidget(groupBoxUser);

        cbCmaster = new QCheckBox(LoginWindow);
        cbCmaster->setObjectName(QString::fromUtf8("cbCmaster"));
        cbCmaster->setChecked(true);

        verticalLayout->addWidget(cbCmaster);

        verticalSpacer = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        pbLogin = new QPushButton(LoginWindow);
        pbLogin->setObjectName(QString::fromUtf8("pbLogin"));

        horizontalLayout->addWidget(pbLogin);

        pbNext = new QPushButton(LoginWindow);
        pbNext->setObjectName(QString::fromUtf8("pbNext"));

        horizontalLayout->addWidget(pbNext);

        pbExit = new QPushButton(LoginWindow);
        pbExit->setObjectName(QString::fromUtf8("pbExit"));

        horizontalLayout->addWidget(pbExit);


        verticalLayout->addLayout(horizontalLayout);


        retranslateUi(LoginWindow);

        pbLogin->setDefault(true);


        QMetaObject::connectSlotsByName(LoginWindow);
    } // setupUi

    void retranslateUi(QDialog *LoginWindow)
    {
        LoginWindow->setWindowTitle(QCoreApplication::translate("LoginWindow", "XTS API Login", nullptr));
        labelTitle->setText(QCoreApplication::translate("LoginWindow", "Trading Terminal Login", nullptr));
        groupBoxMarketData->setTitle(QCoreApplication::translate("LoginWindow", "Market Data API", nullptr));
        labelMDAppKey->setText(QCoreApplication::translate("LoginWindow", "App Key:", nullptr));
        lb_md_appKey->setPlaceholderText(QCoreApplication::translate("LoginWindow", "Enter Market Data App Key", nullptr));
        labelMDSecretKey->setText(QCoreApplication::translate("LoginWindow", "Secret Key:", nullptr));
        lb_md_secretKey->setPlaceholderText(QCoreApplication::translate("LoginWindow", "Enter Market Data Secret Key", nullptr));
        lbMDStatus->setText(QString());
        groupBoxInteractive->setTitle(QCoreApplication::translate("LoginWindow", "Interactive API", nullptr));
        labelIAAppKey->setText(QCoreApplication::translate("LoginWindow", "App Key:", nullptr));
        lb_ia_appKey->setPlaceholderText(QCoreApplication::translate("LoginWindow", "Enter Interactive App Key", nullptr));
        labelIASecretKey->setText(QCoreApplication::translate("LoginWindow", "Secret Key:", nullptr));
        lb_ia_secretKey->setPlaceholderText(QCoreApplication::translate("LoginWindow", "Enter Interactive Secret Key", nullptr));
        lbIAStatus->setText(QString());
        groupBoxUser->setTitle(QCoreApplication::translate("LoginWindow", "User Information", nullptr));
        labelLoginID->setText(QCoreApplication::translate("LoginWindow", "Login ID:", nullptr));
        lb_loginId->setPlaceholderText(QCoreApplication::translate("LoginWindow", "Enter Login ID", nullptr));
        labelClientCode->setText(QCoreApplication::translate("LoginWindow", "Client Code:", nullptr));
        leClient->setPlaceholderText(QCoreApplication::translate("LoginWindow", "Client Code (optional)", nullptr));
        cbCmaster->setText(QCoreApplication::translate("LoginWindow", "Download Master Contracts on Login", nullptr));
        pbLogin->setText(QCoreApplication::translate("LoginWindow", "Login", nullptr));
        pbNext->setText(QCoreApplication::translate("LoginWindow", "Continue", nullptr));
        pbExit->setText(QCoreApplication::translate("LoginWindow", "Exit", nullptr));
    } // retranslateUi

};

namespace Ui {
    class LoginWindow: public Ui_LoginWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LOGINWINDOW_H

/********************************************************************************
** Form generated from reading UI file 'customize.ui'
**
** Created by: Qt User Interface Compiler version 5.14.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CUSTOMIZE_H
#define UI_CUSTOMIZE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFontComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_CustomizeDialog
{
public:
    QVBoxLayout *verticalLayout;
    QGridLayout *gridLayout;
    QLabel *label;
    QFontComboBox *fontComboBox;
    QLabel *label_2;
    QComboBox *colorForComboBox;
    QLabel *label_3;
    QPushButton *colorPickerButton;
    QListWidget *attributeListWidget;
    QLabel *previewTextLabel;
    QHBoxLayout *horizontalLayout;
    QVBoxLayout *verticalLayout_2;
    QCheckBox *titleBarCheckBox;
    QCheckBox *toolBarCheckBox;
    QCheckBox *labelsOnCheckBox;
    QCheckBox *gridCheckBox;
    QGroupBox *groupBox;
    QVBoxLayout *verticalLayout_3;
    QRadioButton *defaultRadio;
    QRadioButton *sizePositionRadio;
    QRadioButton *lastSavedRadio;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *resetButton;
    QSpacerItem *horizontalSpacer;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *CustomizeDialog)
    {
        if (CustomizeDialog->objectName().isEmpty())
            CustomizeDialog->setObjectName(QString::fromUtf8("CustomizeDialog"));
        CustomizeDialog->resize(380, 420);
        verticalLayout = new QVBoxLayout(CustomizeDialog);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        gridLayout = new QGridLayout();
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        label = new QLabel(CustomizeDialog);
        label->setObjectName(QString::fromUtf8("label"));

        gridLayout->addWidget(label, 0, 0, 1, 1);

        fontComboBox = new QFontComboBox(CustomizeDialog);
        fontComboBox->setObjectName(QString::fromUtf8("fontComboBox"));

        gridLayout->addWidget(fontComboBox, 0, 1, 1, 1);

        label_2 = new QLabel(CustomizeDialog);
        label_2->setObjectName(QString::fromUtf8("label_2"));

        gridLayout->addWidget(label_2, 1, 0, 1, 1);

        colorForComboBox = new QComboBox(CustomizeDialog);
        colorForComboBox->addItem(QString());
        colorForComboBox->addItem(QString());
        colorForComboBox->setObjectName(QString::fromUtf8("colorForComboBox"));

        gridLayout->addWidget(colorForComboBox, 1, 1, 1, 1);

        label_3 = new QLabel(CustomizeDialog);
        label_3->setObjectName(QString::fromUtf8("label_3"));

        gridLayout->addWidget(label_3, 2, 0, 1, 1);

        colorPickerButton = new QPushButton(CustomizeDialog);
        colorPickerButton->setObjectName(QString::fromUtf8("colorPickerButton"));

        gridLayout->addWidget(colorPickerButton, 2, 1, 1, 1);


        verticalLayout->addLayout(gridLayout);

        attributeListWidget = new QListWidget(CustomizeDialog);
        new QListWidgetItem(attributeListWidget);
        new QListWidgetItem(attributeListWidget);
        new QListWidgetItem(attributeListWidget);
        attributeListWidget->setObjectName(QString::fromUtf8("attributeListWidget"));

        verticalLayout->addWidget(attributeListWidget);

        previewTextLabel = new QLabel(CustomizeDialog);
        previewTextLabel->setObjectName(QString::fromUtf8("previewTextLabel"));
        previewTextLabel->setMinimumSize(QSize(0, 40));
        previewTextLabel->setFrameShape(QFrame::Box);
        previewTextLabel->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(previewTextLabel);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        titleBarCheckBox = new QCheckBox(CustomizeDialog);
        titleBarCheckBox->setObjectName(QString::fromUtf8("titleBarCheckBox"));

        verticalLayout_2->addWidget(titleBarCheckBox);

        toolBarCheckBox = new QCheckBox(CustomizeDialog);
        toolBarCheckBox->setObjectName(QString::fromUtf8("toolBarCheckBox"));

        verticalLayout_2->addWidget(toolBarCheckBox);

        labelsOnCheckBox = new QCheckBox(CustomizeDialog);
        labelsOnCheckBox->setObjectName(QString::fromUtf8("labelsOnCheckBox"));

        verticalLayout_2->addWidget(labelsOnCheckBox);

        gridCheckBox = new QCheckBox(CustomizeDialog);
        gridCheckBox->setObjectName(QString::fromUtf8("gridCheckBox"));

        verticalLayout_2->addWidget(gridCheckBox);


        horizontalLayout->addLayout(verticalLayout_2);

        groupBox = new QGroupBox(CustomizeDialog);
        groupBox->setObjectName(QString::fromUtf8("groupBox"));
        verticalLayout_3 = new QVBoxLayout(groupBox);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        defaultRadio = new QRadioButton(groupBox);
        defaultRadio->setObjectName(QString::fromUtf8("defaultRadio"));

        verticalLayout_3->addWidget(defaultRadio);

        sizePositionRadio = new QRadioButton(groupBox);
        sizePositionRadio->setObjectName(QString::fromUtf8("sizePositionRadio"));

        verticalLayout_3->addWidget(sizePositionRadio);

        lastSavedRadio = new QRadioButton(groupBox);
        lastSavedRadio->setObjectName(QString::fromUtf8("lastSavedRadio"));

        verticalLayout_3->addWidget(lastSavedRadio);


        horizontalLayout->addWidget(groupBox);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        resetButton = new QPushButton(CustomizeDialog);
        resetButton->setObjectName(QString::fromUtf8("resetButton"));

        horizontalLayout_2->addWidget(resetButton);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);

        buttonBox = new QDialogButtonBox(CustomizeDialog);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Apply|QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        horizontalLayout_2->addWidget(buttonBox);


        verticalLayout->addLayout(horizontalLayout_2);


        retranslateUi(CustomizeDialog);

        QMetaObject::connectSlotsByName(CustomizeDialog);
    } // setupUi

    void retranslateUi(QDialog *CustomizeDialog)
    {
        CustomizeDialog->setWindowTitle(QCoreApplication::translate("CustomizeDialog", "Customize", nullptr));
        label->setText(QCoreApplication::translate("CustomizeDialog", "Font:", nullptr));
        label_2->setText(QCoreApplication::translate("CustomizeDialog", "Color For:", nullptr));
        colorForComboBox->setItemText(0, QCoreApplication::translate("CustomizeDialog", "Background", nullptr));
        colorForComboBox->setItemText(1, QCoreApplication::translate("CustomizeDialog", "Foreground", nullptr));

        label_3->setText(QCoreApplication::translate("CustomizeDialog", "Color Picker:", nullptr));
        colorPickerButton->setText(QCoreApplication::translate("CustomizeDialog", "Select Color", nullptr));

        const bool __sortingEnabled = attributeListWidget->isSortingEnabled();
        attributeListWidget->setSortingEnabled(false);
        QListWidgetItem *___qlistwidgetitem = attributeListWidget->item(0);
        ___qlistwidgetitem->setText(QCoreApplication::translate("CustomizeDialog", "General Appearance", nullptr));
        QListWidgetItem *___qlistwidgetitem1 = attributeListWidget->item(1);
        ___qlistwidgetitem1->setText(QCoreApplication::translate("CustomizeDialog", "Table Grid", nullptr));
        QListWidgetItem *___qlistwidgetitem2 = attributeListWidget->item(2);
        ___qlistwidgetitem2->setText(QCoreApplication::translate("CustomizeDialog", "Labels", nullptr));
        attributeListWidget->setSortingEnabled(__sortingEnabled);

        previewTextLabel->setText(QCoreApplication::translate("CustomizeDialog", "Preview Text", nullptr));
        titleBarCheckBox->setText(QCoreApplication::translate("CustomizeDialog", "Show Title Bar", nullptr));
        toolBarCheckBox->setText(QCoreApplication::translate("CustomizeDialog", "Show Tool Bar", nullptr));
        labelsOnCheckBox->setText(QCoreApplication::translate("CustomizeDialog", "Labels On", nullptr));
        gridCheckBox->setText(QCoreApplication::translate("CustomizeDialog", "Show Grid", nullptr));
        groupBox->setTitle(QCoreApplication::translate("CustomizeDialog", "Open With", nullptr));
        defaultRadio->setText(QCoreApplication::translate("CustomizeDialog", "Default", nullptr));
        sizePositionRadio->setText(QCoreApplication::translate("CustomizeDialog", "Last Size/Pos", nullptr));
        lastSavedRadio->setText(QCoreApplication::translate("CustomizeDialog", "Last Saved Workspace", nullptr));
        resetButton->setText(QCoreApplication::translate("CustomizeDialog", "Reset Defaults", nullptr));
    } // retranslateUi

};

namespace Ui {
    class CustomizeDialog: public Ui_CustomizeDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CUSTOMIZE_H

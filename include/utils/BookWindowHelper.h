#ifndef BOOKWINDOWHELPER_H
#define BOOKWINDOWHELPER_H

#include <QTableView>
#include <QHeaderView>
#include <QAbstractItemModel>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMenu>

class BookWindowHelper {
public:
    /**
     * @brief Common CSV Export logic for book windows
     */
    static void exportToCSV(QTableView* tableView, QAbstractItemModel* model, bool filterRowVisible, QWidget* parent) {
        QString fileName = QFileDialog::getSaveFileName(parent, "Export to CSV", "", "CSV Files (*.csv)");
        if (fileName.isEmpty()) return;

        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

        QTextStream out(&file);
        
        // Write Headers
        for (int c = 0; c < model->columnCount(); ++c) {
            out << model->headerData(c, Qt::Horizontal).toString();
            if (c < model->columnCount() - 1) out << ",";
        }
        out << "\n";

        // Write Rows
        for (int r = 0; r < model->rowCount(); ++r) {
            if (filterRowVisible && r == 0) continue; // Skip filter row

            for (int c = 0; c < model->columnCount(); ++c) {
                out << model->data(model->index(r, c), Qt::DisplayRole).toString().replace(",", " ");
                if (c < model->columnCount() - 1) out << ",";
            }
            out << "\n";
        }
        file.close();
    }
};

#endif // BOOKWINDOWHELPER_H

#include "models/TokenAddressBook.h"
#include <QDebug>

TokenAddressBook::TokenAddressBook(QObject *parent)
    : QObject(parent)
{
}

QString TokenAddressBook::makeKey(const QString& exchange, const QString& client, int token)
{
    return QString("%1:%2:%3").arg(exchange).arg(client).arg(token);
}

void TokenAddressBook::addToken(int token, int row)
{
    // For simple tokens, we use "token" as the key
    QString key = QString::number(token);
    
    if (!m_keyToRows.contains(key)) m_keyToRows[key] = QList<int>();
    if (!m_keyToRows[key].contains(row)) {
        m_keyToRows[key].append(row);
    }
    m_rowToKey[row] = key;
    
    emit tokenAdded(key, row);
}

void TokenAddressBook::addCompositeToken(const QString& exchange, const QString& client, int token, int row)
{
    QString key = makeKey(exchange, client, token);
    
    if (!m_keyToRows.contains(key)) m_keyToRows[key] = QList<int>();
    if (!m_keyToRows[key].contains(row)) {
        m_keyToRows[key].append(row);
    }
    m_rowToKey[row] = key;
    
    emit tokenAdded(key, row);
}

void TokenAddressBook::removeToken(int token, int row)
{
    QString key = QString::number(token);
    if (m_keyToRows.contains(key)) {
        m_keyToRows[key].removeAll(row);
        if (m_keyToRows[key].isEmpty()) m_keyToRows.remove(key);
    }
    if (m_rowToKey.value(row) == key) m_rowToKey.remove(row);
    emit tokenRemoved(key, row);
}

void TokenAddressBook::removeCompositeToken(const QString& exchange, const QString& client, int token, int row)
{
    QString key = makeKey(exchange, client, token);
    if (m_keyToRows.contains(key)) {
        m_keyToRows[key].removeAll(row);
        if (m_keyToRows[key].isEmpty()) m_keyToRows.remove(key);
    }
    if (m_rowToKey.value(row) == key) m_rowToKey.remove(row);
    emit tokenRemoved(key, row);
}

void TokenAddressBook::clear()
{
    m_keyToRows.clear();
    m_rowToKey.clear();
    emit cleared();
}

void TokenAddressBook::onRowMoved(int fromRow, int toRow)
{
    QString key = m_rowToKey.value(fromRow);
    if (key.isEmpty()) return;
    
    m_rowToKey.remove(fromRow);
    m_rowToKey[toRow] = key;
    
    if (m_keyToRows.contains(key)) {
        int idx = m_keyToRows[key].indexOf(fromRow);
        if (idx != -1) m_keyToRows[key][idx] = toRow;
    }
}

void TokenAddressBook::onRowsInserted(int firstRow, int count)
{
    updateRowIndices(firstRow, count);
}

void TokenAddressBook::onRowsRemoved(int firstRow, int count)
{
    for (int row = firstRow; row < firstRow + count; ++row) {
        QString key = m_rowToKey.value(row);
        if (!key.isEmpty()) {
            m_keyToRows[key].removeAll(row);
            if (m_keyToRows[key].isEmpty()) m_keyToRows.remove(key);
            m_rowToKey.remove(row);
        }
    }
    updateRowIndices(firstRow + count, -count);
}

QList<int> TokenAddressBook::getRowsForToken(int token) const
{
    return m_keyToRows.value(QString::number(token));
}

QList<int> TokenAddressBook::getRowsForCompositeToken(const QString& exchange, const QString& client, int token) const
{
    return m_keyToRows.value(makeKey(exchange, client, token));
}

int TokenAddressBook::getTokenForRow(int row) const
{
    QString key = m_rowToKey.value(row);
    if (key.isEmpty()) return -1;
    // If key contains ':', it's a composite key, we extract the last part
    int lastColon = key.lastIndexOf(':');
    if (lastColon != -1) {
        return key.mid(lastColon + 1).toInt();
    }
    return key.toInt();
}

QString TokenAddressBook::getCompositeKeyForRow(int row) const
{
    return m_rowToKey.value(row);
}

bool TokenAddressBook::hasToken(int token) const
{
    return m_keyToRows.contains(QString::number(token));
}

bool TokenAddressBook::hasCompositeToken(const QString& exchange, const QString& client, int token) const
{
    return m_keyToRows.contains(makeKey(exchange, client, token));
}

void TokenAddressBook::updateRowIndices(int startRow, int delta)
{
    if (delta == 0) return;
    
    QMap<int, QString> newRowToKey;
    QMap<QString, QList<int>> newKeyToRows;
    
    for (auto it = m_rowToKey.constBegin(); it != m_rowToKey.constEnd(); ++it) {
        int row = it.key();
        QString key = it.value();
        if (row >= startRow) {
            int newRow = row + delta;
            if (newRow >= 0) newRowToKey[newRow] = key;
        } else {
            newRowToKey[row] = key;
        }
    }
    
    for (auto it = m_keyToRows.constBegin(); it != m_keyToRows.constEnd(); ++it) {
        QString key = it.key();
        QList<int> rows = it.value();
        QList<int> newRows;
        for (int row : rows) {
            if (row >= startRow) {
                int newRow = row + delta;
                if (newRow >= 0) newRows.append(newRow);
            } else {
                newRows.append(row);
            }
        }
        if (!newRows.isEmpty()) newKeyToRows[key] = newRows;
    }
    
    m_rowToKey = newRowToKey;
    m_keyToRows = newKeyToRows;
}

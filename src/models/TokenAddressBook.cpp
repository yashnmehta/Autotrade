#include "models/domain/TokenAddressBook.h"
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

// === Optimized Int64 Implementation ===

void TokenAddressBook::addIntKeyToken(int exchangeSegment, int token, int row)
{
    int64_t key = makeIntKey(exchangeSegment, token);
    
    if (!m_intKeyToRows.contains(key)) m_intKeyToRows[key] = QList<int>();
    if (!m_intKeyToRows[key].contains(row)) {
        m_intKeyToRows[key].append(row);
    }
    m_rowToIntKey[row] = key;
}

void TokenAddressBook::removeIntKeyToken(int exchangeSegment, int token, int row)
{
    int64_t key = makeIntKey(exchangeSegment, token);
    
    if (m_intKeyToRows.contains(key)) {
        m_intKeyToRows[key].removeAll(row);
        if (m_intKeyToRows[key].isEmpty()) m_intKeyToRows.remove(key);
    }
    if (m_rowToIntKey.value(row) == key) m_rowToIntKey.remove(row);
}

QList<int> TokenAddressBook::getRowsForIntKey(int exchangeSegment, int token) const
{
    // Direct hash lookup - O(1) / O(log N) for QMap
    return m_intKeyToRows.value(makeIntKey(exchangeSegment, token));
}

QList<int> TokenAddressBook::getRowsForIntKey(int64_t key) const
{
    return m_intKeyToRows.value(key);
}

int64_t TokenAddressBook::getIntKeyForRow(int row) const
{
    return m_rowToIntKey.value(row, 0); // 0 is invalid key
}

void TokenAddressBook::clear()
{
    m_keyToRows.clear();
    m_rowToKey.clear();
    m_intKeyToRows.clear();
    m_rowToIntKey.clear();
    emit cleared();
}

void TokenAddressBook::onRowMoved(int fromRow, int toRow)
{
    // Handle String Keys
    QString key = m_rowToKey.value(fromRow);
    if (!key.isEmpty()) {
        m_rowToKey.remove(fromRow);
        m_rowToKey[toRow] = key;
        
        if (m_keyToRows.contains(key)) {
            int idx = m_keyToRows[key].indexOf(fromRow);
            if (idx != -1) m_keyToRows[key][idx] = toRow;
        }
    }
    
    // Handle Int64 Keys
    int64_t intKey = m_rowToIntKey.value(fromRow, 0);
    if (intKey != 0) {
        m_rowToIntKey.remove(fromRow);
        m_rowToIntKey[toRow] = intKey;
        
        if (m_intKeyToRows.contains(intKey)) {
            int idx = m_intKeyToRows[intKey].indexOf(fromRow);
            if (idx != -1) m_intKeyToRows[intKey][idx] = toRow;
        }
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
        
        // Remove Int Key
        int64_t intKey = m_rowToIntKey.value(row, 0);
        if (intKey != 0) {
            m_intKeyToRows[intKey].removeAll(row);
            if (m_intKeyToRows[intKey].isEmpty()) m_intKeyToRows.remove(intKey);
            m_rowToIntKey.remove(row);
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
    
    // Update String Key Maps
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
    
    // Update Int64 Key Maps
    QMap<int, int64_t> newRowToIntKey;
    QMap<int64_t, QList<int>> newIntKeyToRows;
    
    for (auto it = m_rowToIntKey.constBegin(); it != m_rowToIntKey.constEnd(); ++it) {
        int row = it.key();
        int64_t key = it.value();
        if (row >= startRow) {
            int newRow = row + delta;
            if (newRow >= 0) newRowToIntKey[newRow] = key;
        } else {
            newRowToIntKey[row] = key;
        }
    }
    
    for (auto it = m_intKeyToRows.constBegin(); it != m_intKeyToRows.constEnd(); ++it) {
        int64_t key = it.key();
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
        if (!newRows.isEmpty()) newIntKeyToRows[key] = newRows;
    }
    
    m_rowToIntKey = newRowToIntKey;
    m_intKeyToRows = newIntKeyToRows;
}

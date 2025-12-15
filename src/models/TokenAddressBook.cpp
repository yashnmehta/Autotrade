#include "models/TokenAddressBook.h"
#include <QDebug>

TokenAddressBook::TokenAddressBook(QObject *parent)
    : QObject(parent)
{
}

void TokenAddressBook::addToken(int token, int row)
{
    if (token <= 0) {
        qWarning() << "[TokenAddressBook] Attempted to add invalid token:" << token;
        return;
    }
    
    // Add to token -> rows mapping
    if (!m_tokenToRows.contains(token)) {
        m_tokenToRows[token] = QList<int>();
    }
    
    if (!m_tokenToRows[token].contains(row)) {
        m_tokenToRows[token].append(row);
        qDebug() << "[TokenAddressBook] Added token" << token << "at row" << row;
    } else {
        qWarning() << "[TokenAddressBook] Token" << token << "already exists at row" << row;
    }
    
    // Add to row -> token mapping
    if (m_rowToToken.contains(row) && m_rowToToken[row] != token) {
        qWarning() << "[TokenAddressBook] Row" << row << "already has token"
                   << m_rowToToken[row] << ", replacing with" << token;
    }
    m_rowToToken[row] = token;
    
    emit tokenAdded(token, row);
}

void TokenAddressBook::removeToken(int token, int row)
{
    bool removed = false;
    
    // Remove from token -> rows mapping
    if (m_tokenToRows.contains(token)) {
        int beforeSize = m_tokenToRows[token].size();
        m_tokenToRows[token].removeAll(row);
        int afterSize = m_tokenToRows[token].size();
        
        removed = (beforeSize != afterSize);
        
        // Clean up empty token entries
        if (m_tokenToRows[token].isEmpty()) {
            m_tokenToRows.remove(token);
        }
    }
    
    // Remove from row -> token mapping
    if (m_rowToToken.contains(row) && m_rowToToken[row] == token) {
        m_rowToToken.remove(row);
        removed = true;
    }
    
    if (removed) {
        qDebug() << "[TokenAddressBook] Removed token" << token << "from row" << row;
        emit tokenRemoved(token, row);
    } else {
        qWarning() << "[TokenAddressBook] Token" << token << "not found at row" << row;
    }
}

void TokenAddressBook::clear()
{
    m_tokenToRows.clear();
    m_rowToToken.clear();
    qDebug() << "[TokenAddressBook] Cleared all mappings";
    emit cleared();
}

void TokenAddressBook::onRowMoved(int fromRow, int toRow)
{
    int token = m_rowToToken.value(fromRow, -1);
    if (token == -1) {
        return;  // No token at source row (blank row)
    }
    
    // Update row -> token mapping
    m_rowToToken.remove(fromRow);
    m_rowToToken[toRow] = token;
    
    // Update token -> rows mapping
    if (m_tokenToRows.contains(token)) {
        int idx = m_tokenToRows[token].indexOf(fromRow);
        if (idx != -1) {
            m_tokenToRows[token][idx] = toRow;
        }
    }
    
    qDebug() << "[TokenAddressBook] Moved token" << token
             << "from row" << fromRow << "to row" << toRow;
}

void TokenAddressBook::onRowsInserted(int firstRow, int count)
{
    // Shift all row indices >= firstRow by +count
    updateRowIndices(firstRow, count);
    qDebug() << "[TokenAddressBook] Rows inserted at" << firstRow << "count:" << count;
}

void TokenAddressBook::onRowsRemoved(int firstRow, int count)
{
    // Remove tokens for deleted rows
    for (int row = firstRow; row < firstRow + count; ++row) {
        int token = m_rowToToken.value(row, -1);
        if (token != -1) {
            removeToken(token, row);
        }
    }
    
    // Shift all row indices > firstRow by -count
    updateRowIndices(firstRow + count, -count);
    qDebug() << "[TokenAddressBook] Rows removed at" << firstRow << "count:" << count;
}

QList<int> TokenAddressBook::getRowsForToken(int token) const
{
    return m_tokenToRows.value(token, QList<int>());
}

int TokenAddressBook::getTokenForRow(int row) const
{
    return m_rowToToken.value(row, -1);
}

bool TokenAddressBook::hasToken(int token) const
{
    return m_tokenToRows.contains(token);
}

bool TokenAddressBook::hasRow(int row) const
{
    return m_rowToToken.contains(row);
}

void TokenAddressBook::dump() const
{
    qDebug() << "╔═══════════════════════════════════════════════════════╗";
    qDebug() << "║       TokenAddressBook Dump                           ║";
    qDebug() << "╠═══════════════════════════════════════════════════════╣";
    
    qDebug() << "║ Token -> Rows Mapping:";
    if (m_tokenToRows.isEmpty()) {
        qDebug() << "║   (empty)";
    } else {
        for (auto it = m_tokenToRows.constBegin(); it != m_tokenToRows.constEnd(); ++it) {
            QString rowsStr;
            for (int row : it.value()) {
                if (!rowsStr.isEmpty()) rowsStr += ", ";
                rowsStr += QString::number(row);
            }
            qDebug() << "║   Token" << it.key() << "-> Rows [" << rowsStr << "]";
        }
    }
    
    qDebug() << "║";
    qDebug() << "║ Row -> Token Mapping:";
    if (m_rowToToken.isEmpty()) {
        qDebug() << "║   (empty)";
    } else {
        for (auto it = m_rowToToken.constBegin(); it != m_rowToToken.constEnd(); ++it) {
            qDebug() << "║   Row" << it.key() << "-> Token" << it.value();
        }
    }
    
    qDebug() << "║";
    qDebug() << "║ Statistics:";
    qDebug() << "║   Unique tokens:" << m_tokenToRows.size();
    qDebug() << "║   Rows with tokens:" << m_rowToToken.size();
    qDebug() << "╚═══════════════════════════════════════════════════════╝";
}

void TokenAddressBook::updateRowIndices(int startRow, int delta)
{
    if (delta == 0) return;
    
    // Create new mappings with updated row indices
    QMap<int, int> newRowToToken;
    QMap<int, QList<int>> newTokenToRows;
    
    // Update row -> token mapping
    for (auto it = m_rowToToken.constBegin(); it != m_rowToToken.constEnd(); ++it) {
        int row = it.key();
        int token = it.value();
        
        if (row >= startRow) {
            int newRow = row + delta;
            if (newRow >= 0) {  // Only keep valid rows
                newRowToToken[newRow] = token;
            }
        } else {
            newRowToToken[row] = token;
        }
    }
    
    // Update token -> rows mapping
    for (auto it = m_tokenToRows.constBegin(); it != m_tokenToRows.constEnd(); ++it) {
        int token = it.key();
        QList<int> rows = it.value();
        
        QList<int> newRows;
        for (int row : rows) {
            if (row >= startRow) {
                int newRow = row + delta;
                if (newRow >= 0) {  // Only keep valid rows
                    newRows.append(newRow);
                }
            } else {
                newRows.append(row);
            }
        }
        
        if (!newRows.isEmpty()) {
            newTokenToRows[token] = newRows;
        }
    }
    
    m_rowToToken = newRowToToken;
    m_tokenToRows = newTokenToRows;
    
    qDebug() << "[TokenAddressBook] Updated row indices from" << startRow
             << "by delta" << delta;
}

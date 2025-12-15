#ifndef TOKENADDRESSBOOK_H
#define TOKENADDRESSBOOK_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QSet>

/**
 * @brief Fast bidirectional mapping between tokens and row indices
 * 
 * Provides O(1) lookups for price updates and token validation.
 * Automatically maintains consistency when rows are added/removed/moved.
 * 
 * Usage:
 * - Add token when scrip is added: addToken(token, row)
 * - Remove token when scrip is removed: removeToken(token, row)
 * - Find rows for a token: getRowsForToken(token)
 * - Check if token exists: hasToken(token)
 */
class TokenAddressBook : public QObject
{
    Q_OBJECT

public:
    explicit TokenAddressBook(QObject *parent = nullptr);
    virtual ~TokenAddressBook() = default;
    
    // === Token Management ===
    
    /**
     * @brief Add a token-to-row mapping
     * @param token Unique token ID
     * @param row Row index in the model
     */
    void addToken(int token, int row);
    
    /**
     * @brief Remove a token-to-row mapping
     * @param token Unique token ID
     * @param row Row index in the model
     */
    void removeToken(int token, int row);
    
    /**
     * @brief Clear all mappings
     */
    void clear();
    
    // === Row Operation Handlers ===
    
    /**
     * @brief Update mappings when a row is moved
     * @param fromRow Source row index
     * @param toRow Destination row index
     */
    void onRowMoved(int fromRow, int toRow);
    
    /**
     * @brief Update mappings when rows are inserted
     * @param firstRow First inserted row
     * @param count Number of rows inserted
     */
    void onRowsInserted(int firstRow, int count);
    
    /**
     * @brief Update mappings when rows are removed
     * @param firstRow First removed row
     * @param count Number of rows removed
     */
    void onRowsRemoved(int firstRow, int count);
    
    // === Lookups (O(1) performance) ===
    
    /**
     * @brief Get all row indices for a given token
     * @param token Token ID to search for
     * @return List of row indices (usually just one, but supports multiple)
     */
    QList<int> getRowsForToken(int token) const;
    
    /**
     * @brief Get the token for a given row
     * @param row Row index
     * @return Token ID, or -1 if row has no token (e.g., blank row)
     */
    int getTokenForRow(int row) const;
    
    /**
     * @brief Check if a token exists in the address book
     * @param token Token ID to check
     * @return true if token is registered
     */
    bool hasToken(int token) const;
    
    /**
     * @brief Check if a row has a token assigned
     * @param row Row index
     * @return true if row has a valid token
     */
    bool hasRow(int row) const;
    
    // === Statistics ===
    
    /**
     * @brief Get the number of unique tokens registered
     */
    int tokenCount() const { return m_tokenToRows.size(); }
    
    /**
     * @brief Get the number of rows with tokens assigned
     */
    int rowCount() const { return m_rowToToken.size(); }
    
    /**
     * @brief Get all registered tokens
     */
    QList<int> getAllTokens() const { return m_tokenToRows.keys(); }
    
    // === Debug ===
    
    /**
     * @brief Dump the address book contents to qDebug
     */
    void dump() const;

signals:
    /**
     * @brief Emitted when a token is added
     */
    void tokenAdded(int token, int row);
    
    /**
     * @brief Emitted when a token is removed
     */
    void tokenRemoved(int token, int row);
    
    /**
     * @brief Emitted when mappings are cleared
     */
    void cleared();

private:
    /**
     * @brief Update all row indices >= startRow by delta
     * @param startRow Starting row for update
     * @param delta Amount to shift (positive for insert, negative for remove)
     */
    void updateRowIndices(int startRow, int delta);
    
    // Token -> List of row indices
    // (List instead of single int to support future multi-watch scenarios)
    QMap<int, QList<int>> m_tokenToRows;
    
    // Row -> Token ID
    QMap<int, int> m_rowToToken;
};

#endif // TOKENADDRESSBOOK_H

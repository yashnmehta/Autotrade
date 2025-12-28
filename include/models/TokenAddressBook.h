#ifndef TOKENADDRESSBOOK_H
#define TOKENADDRESSBOOK_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QString>

/**
 * @brief Fast bidirectional mapping between instrument identifiers and row indices
 * 
 * Supports both simple token-based lookups and composite keys (exchange+client+token).
 * Composite keys are handled as formatted strings: "exchange:client:token".
 */
class TokenAddressBook : public QObject
{
    Q_OBJECT

public:
    explicit TokenAddressBook(QObject *parent = nullptr);
    virtual ~TokenAddressBook() = default;
    
    // === Management ===
    
    /**
     * @brief Add a mapping for a simple token (Market Watch style)
     */
    void addToken(int token, int row);

    /**
     * @brief Add a mapping for a composite key (Position style)
     */
    void addCompositeToken(const QString& exchange, const QString& client, int token, int row);
    
    /**
     * @brief Remove a mapping
     */
    void removeToken(int token, int row);
    void removeCompositeToken(const QString& exchange, const QString& client, int token, int row);
    
    void clear();
    
    // === Row Operation Handlers ===
    
    void onRowMoved(int fromRow, int toRow);
    void onRowsInserted(int firstRow, int count);
    void onRowsRemoved(int firstRow, int count);
    
    // === Lookups ===
    
    QList<int> getRowsForToken(int token) const;
    QList<int> getRowsForCompositeToken(const QString& exchange, const QString& client, int token) const;
    
    int getTokenForRow(int row) const;
    QString getCompositeKeyForRow(int row) const;
    
    bool hasToken(int token) const;
    bool hasCompositeToken(const QString& exchange, const QString& client, int token) const;
    
    // === Utils ===
    static QString makeKey(const QString& exchange, const QString& client, int token);

signals:
    void tokenAdded(const QString& key, int row);
    void tokenRemoved(const QString& key, int row);
    void cleared();

private:
    void updateRowIndices(int startRow, int delta);
    
    // key -> list of rows
    QMap<QString, QList<int>> m_keyToRows;
    // row -> key
    QMap<int, QString> m_rowToKey;
};

#endif // TOKENADDRESSBOOK_H

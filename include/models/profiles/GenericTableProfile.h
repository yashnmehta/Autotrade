#ifndef GENERICTABLEPROFILE_H
#define GENERICTABLEPROFILE_H

#include <QString>
#include <QList>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <Qt>

/**
 * @brief Column metadata shared between all column-profile consumers.
 *
 * Each window type (MarketWatch, OptionChain, OrderBook, …) builds its own
 * QList<GenericColumnInfo> describing the columns it supports.
 */
struct GenericColumnInfo {
    int id;                  ///< Unique numeric column ID (maps to window-specific enum)
    QString name;            ///< Human-readable name shown in profile dialog
    QString shortName;       ///< Compact name (used in very narrow columns)
    QString description;     ///< Tooltip / descriptive text
    int defaultWidth;        ///< Default pixel width
    Qt::Alignment alignment; ///< Text alignment
    bool visibleByDefault;   ///< Visible in the "Default" profile
    bool isNumeric;          ///< Is numeric data

    GenericColumnInfo()
        : id(0), defaultWidth(80)
        , alignment(Qt::AlignRight | Qt::AlignVCenter)
        , visibleByDefault(true), isNumeric(true) {}

    GenericColumnInfo(int _id, const QString &_name, int _width, bool _visible)
        : id(_id), name(_name), shortName(_name)
        , defaultWidth(_width)
        , alignment(Qt::AlignRight | Qt::AlignVCenter)
        , visibleByDefault(_visible), isNumeric(true) {}
};

/**
 * @brief Generic column profile storing visibility, order and widths.
 *
 * Used by every window that supports column customisation:
 * MarketWatch, OptionChain, OrderBook, TradeBook, etc.
 *
 * Internally uses plain `int` column IDs so the same class works across
 * all window types.
 */
class GenericTableProfile
{
public:
    GenericTableProfile(const QString &name = "Default") : m_name(name) {}

    // --- Name / Description ------------------------------------------------
    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    QString description() const { return m_description; }
    void setDescription(const QString &d) { m_description = d; }

    // --- Visibility --------------------------------------------------------
    bool isColumnVisible(int colId) const { return m_visibility.value(colId, true); }
    void setColumnVisible(int colId, bool visible) { m_visibility[colId] = visible; }

    QList<int> visibleColumns() const {
        QList<int> result;
        for (int id : m_order) {
            if (isColumnVisible(id))
                result.append(id);
        }
        return result;
    }

    int visibleColumnCount() const { return visibleColumns().size(); }

    // --- Column Order ------------------------------------------------------
    QList<int> columnOrder() const { return m_order; }
    void setColumnOrder(const QList<int> &order) { m_order = order; }

    // --- Column Width ------------------------------------------------------
    int columnWidth(int colId) const { return m_widths.value(colId, 100); }
    void setColumnWidth(int colId, int width) { m_widths[colId] = width; }

    // --- Serialization -----------------------------------------------------
    QJsonObject toJson() const {
        QJsonObject json;
        json["name"] = m_name;
        json["description"] = m_description;

        QJsonObject visJson;
        for (auto it = m_visibility.begin(); it != m_visibility.end(); ++it)
            visJson[QString::number(it.key())] = it.value();
        json["visibility"] = visJson;

        QJsonObject widthJson;
        for (auto it = m_widths.begin(); it != m_widths.end(); ++it)
            widthJson[QString::number(it.key())] = it.value();
        json["widths"] = widthJson;

        QJsonArray orderArray;
        for (int id : m_order) orderArray.append(id);
        json["order"] = orderArray;

        return json;
    }

    void fromJson(const QJsonObject &json) {
        m_name = json["name"].toString();
        m_description = json["description"].toString();

        m_visibility.clear();
        QJsonObject visJson = json["visibility"].toObject();
        for (const QString &key : visJson.keys())
            m_visibility[key.toInt()] = visJson[key].toBool();

        m_widths.clear();
        QJsonObject widthJson = json["widths"].toObject();
        for (const QString &key : widthJson.keys())
            m_widths[key.toInt()] = widthJson[key].toInt();

        m_order.clear();
        QJsonArray orderArray = json["order"].toArray();
        if (orderArray.isEmpty())
            orderArray = json["columnOrder"].toArray();  // Legacy MarketWatch compat
        for (const QJsonValue &val : orderArray)
            m_order.append(val.toInt());
    }

    // --- Build a "Default" profile from column metadata --------------------
    static GenericTableProfile createDefault(const QList<GenericColumnInfo> &columns) {
        GenericTableProfile p("Default");
        p.setDescription("Default column layout");
        QList<int> order;
        for (const auto &c : columns) {
            order.append(c.id);
            p.setColumnVisible(c.id, c.visibleByDefault);
            p.setColumnWidth(c.id, c.defaultWidth);
        }
        p.setColumnOrder(order);
        return p;
    }

private:
    QString m_name;
    QString m_description;
    QMap<int, bool> m_visibility;
    QMap<int, int> m_widths;
    QList<int> m_order;
};

#endif // GENERICTABLEPROFILE_H

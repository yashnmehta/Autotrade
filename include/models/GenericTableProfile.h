#ifndef GENERICTABLEPROFILE_H
#define GENERICTABLEPROFILE_H

#include <QString>
#include <QList>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

struct GenericColumnInfo {
    int id;
    QString name;
    int defaultWidth;
    bool visibleByDefault;
};

class GenericTableProfile
{
public:
    GenericTableProfile(const QString &name = "Default") : m_name(name) {}

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    bool isColumnVisible(int colId) const { return m_visibility.value(colId, true); }
    void setColumnVisible(int colId, bool visible) { m_visibility[colId] = visible; }

    int columnWidth(int colId) const { return m_widths.value(colId, 100); }
    void setColumnWidth(int colId, int width) { m_widths[colId] = width; }

    QList<int> columnOrder() const { return m_order; }
    void setColumnOrder(const QList<int> &order) { m_order = order; }

    QJsonObject toJson() const {
        QJsonObject json;
        json["name"] = m_name;
        
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
        for (const QJsonValue &val : orderArray)
            m_order.append(val.toInt());
    }

private:
    QString m_name;
    QMap<int, bool> m_visibility;
    QMap<int, int> m_widths;
    QList<int> m_order;
};

#endif // GENERICTABLEPROFILE_H

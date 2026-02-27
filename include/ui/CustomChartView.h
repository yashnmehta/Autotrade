#ifndef CUSTOMCHARTVIEW_H
#define CUSTOMCHARTVIEW_H

#include <QtCharts/QChartView>
#include <QWheelEvent>
#include <QMouseEvent>

/**
 * @brief Custom chart view with scroll-based zoom
 */
class CustomChartView : public QChartView {
  Q_OBJECT

public:
  explicit CustomChartView(QChart *chart, QWidget *parent = nullptr);
  
  void setZoomEnabled(bool enabled) { m_zoomEnabled = enabled; }

signals:
  void zoomChanged(double factor);

protected:
  void wheelEvent(QWheelEvent *event) override;

private:
  bool m_zoomEnabled = true;
};

#endif // CUSTOMCHARTVIEW_H

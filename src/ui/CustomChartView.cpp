#include "ui/CustomChartView.h"
#include <QtCharts/QChart>
#include <QDebug>

CustomChartView::CustomChartView(QChart *chart, QWidget *parent)
    : QChartView(chart, parent) {
  setRenderHint(QPainter::Antialiasing, true);
  setRenderHint(QPainter::TextAntialiasing, true);
  setRenderHint(QPainter::SmoothPixmapTransform, true);
  setRubberBand(QChartView::RectangleRubberBand);
  setInteractive(true);
  setCursor(Qt::CrossCursor);
}

void CustomChartView::wheelEvent(QWheelEvent *event) {
  if (!m_zoomEnabled) {
    QChartView::wheelEvent(event);
    return;
  }

  // Scroll-based zoom
  double factor = 1.0;
  if (event->angleDelta().y() > 0) {
    factor = 0.9; // Zoom in (show fewer candles)
  } else {
    factor = 1.1; // Zoom out (show more candles)
  }

  emit zoomChanged(factor);
  event->accept();
}

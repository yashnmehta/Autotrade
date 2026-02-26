#include "strategies/StrategyFactory.h"
#include "strategies/StrategyBase.h"
#include "strategies/TemplateStrategy.h"
#include <QDebug>

StrategyBase *StrategyFactory::createStrategy(const QString &type,
                                              QObject *parent) {
  // ── Template-based strategies ──
  // Any strategy created from a StrategyTemplate uses the generic
  // TemplateStrategy runtime. The type string is "template" or
  // "template:<templateId>".
  if (type == "template" || type.startsWith("template:")) {
    qDebug() << "[StrategyFactory] Creating TemplateStrategy for type:" << type;
    return new TemplateStrategy(parent);
  }

  // ── Future: Hard-coded strategy types ──
  // Add specific strategy implementations here:
  // if (type == "rsi_reversal") return new RsiReversalStrategy(parent);
  // if (type == "vwap_scalper") return new VwapScalperStrategy(parent);

  qDebug() << "[StrategyFactory] Unknown strategy type:" << type
           << "- Returning nullptr.";
  return nullptr;
}

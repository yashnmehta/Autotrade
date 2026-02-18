#include "strategies/StrategyFactory.h"
#include "strategies/CustomStrategy.h"
#include "strategies/JodiATMStrategy.h"
#include "strategies/MomentumStrategy.h"
#include "strategies/RangeBreakoutStrategy.h"
#include <QDebug>


StrategyBase *StrategyFactory::createStrategy(const QString &type,
                                              QObject *parent) {
  QString t = type.toLower().trimmed().remove("-").remove(" ");

  if (t == "custom" || t == "customstrategy") {
    return new CustomStrategy(parent);
  } else if (t == "momentum" || t == "tspecial") {
    return new MomentumStrategy(parent);
  } else if (t == "rangebreakout" || t == "vixmonkey") {
    return new RangeBreakoutStrategy(parent);
  } else if (t == "jodiatm") {
    return new JodiATMStrategy(parent);
  } else {
    qDebug() << "Unknown strategy type:" << type
             << ", defaulting to MomentumStrategy";
    return new MomentumStrategy(parent);
  }
}

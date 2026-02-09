#ifndef STRATEGY_FACTORY_H
#define STRATEGY_FACTORY_H

#include <QObject>
#include <QString>


class StrategyBase;

class StrategyFactory {
public:
  static StrategyBase *createStrategy(const QString &type,
                                      QObject *parent = nullptr);
};

#endif // STRATEGY_FACTORY_H

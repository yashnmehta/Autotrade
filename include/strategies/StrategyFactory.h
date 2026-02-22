#ifndef STRATEGY_FACTORY_H
#define STRATEGY_FACTORY_H

#include <QString>

class QObject;
class StrategyBase;

/**
 * @brief Factory for creating strategy runtime instances by type string.
 * Types will be registered here as new strategy implementations are added.
 */
class StrategyFactory {
public:
  static StrategyBase *createStrategy(const QString &type,
                                      QObject *parent = nullptr);
};

#endif // STRATEGY_FACTORY_H

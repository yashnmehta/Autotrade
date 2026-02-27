#include "strategy/runtime/OptionsExecutionEngine.h"
#include "utils/ATMCalculator.h"
#include "repository/RepositoryManager.h"
#include <QDebug>
#include <QDate>

// ═══════════════════════════════════════════════════════════
// ✅ REUSE: ATM Strike Resolution (POC Task 3.1)
// Uses existing ATMCalculator + RepositoryManager
// See: 08_BACKEND_AUDIT_FINDINGS.md
// ═══════════════════════════════════════════════════════════

int OptionsExecutionEngine::resolveATMStrike(const QString& symbol,
                                             const QString& expiry,
                                             double spotPrice,
                                             int offset) {
  auto* repo = RepositoryManager::getInstance();
  
  // Step 1: Get sorted strikes from cache (O(1) hash lookup)
  const QVector<double>& strikes = 
      repo->getStrikesForSymbolExpiry(symbol, expiry);
  
  if (strikes.isEmpty()) {
    qCritical() << "[OptionsEngine] No strikes found for" << symbol << expiry;
    return 0;
  }
  
  // Step 2: Find nearest strike (O(log n) binary search)
  auto result = ATMCalculator::calculateFromActualStrikes(spotPrice, strikes, 0);
  
  if (!result.isValid) {
    qCritical() << "[OptionsEngine] ATM calculation failed for spot:" << spotPrice;
    return 0;
  }
  
  double atmStrike = result.atmStrike;
  
  // Step 3: Apply offset (ATM+1, ATM-2, etc.)
  if (offset != 0) {
    atmStrike = applyStrikeOffset(strikes, atmStrike, offset);
  }
  
  qDebug() << "[OptionsEngine] ATM Resolution:"
           << "Spot=" << spotPrice
           << "→ ATM=" << atmStrike
           << "(offset=" << offset << ")";
  
  return static_cast<int>(atmStrike);
}

double OptionsExecutionEngine::applyStrikeOffset(const QVector<double>& strikes,
                                                 double atmStrike,
                                                 int offset) {
  // Find current ATM index
  int currentIdx = strikes.indexOf(atmStrike);
  if (currentIdx == -1) {
    qWarning() << "[OptionsEngine] ATM strike" << atmStrike << "not found in array";
    return atmStrike;
  }
  
  // Apply offset
  int targetIdx = currentIdx + offset;
  
  // Boundary check
  if (targetIdx < 0) {
    qWarning() << "[OptionsEngine] Strike offset" << offset << "below range, using first strike";
    return strikes.first();
  }
  if (targetIdx >= strikes.size()) {
    qWarning() << "[OptionsEngine] Strike offset" << offset << "above range, using last strike";
    return strikes.last();
  }
  
  return strikes[targetIdx];
}

// ═══════════════════════════════════════════════════════════
// Symbol Building (POC Implementation)
// ═══════════════════════════════════════════════════════════

QString OptionsExecutionEngine::buildOptionSymbol(const QString& symbol,
                                                  int strike,
                                                  const QString& optionType,
                                                  const QString& expiry) {
  // For POC: Simple format "NIFTY24550CE"
  // Production may need: "NIFTY2413024550CE" or exchange-specific formats
  
  QString tradingSymbol = symbol + QString::number(strike) + optionType.toUpper();
  
  qDebug() << "[OptionsEngine] Built symbol:" << tradingSymbol
           << "(" << symbol << strike << optionType << expiry << ")";
  
  return tradingSymbol;
}

// ═══════════════════════════════════════════════════════════
// ✅ REUSE: Token Lookup (Wrapper around RepositoryManager)
// ═══════════════════════════════════════════════════════════

int64_t OptionsExecutionEngine::getContractToken(const QString& symbol,
                                                 const QString& expiry,
                                                 double strike,
                                                 const QString& optionType) {
  auto* repo = RepositoryManager::getInstance();
  
  // Get CE/PE token pair from cache (O(1) hash lookup)
  auto tokens = repo->getTokensForStrike(symbol, expiry, strike);
  
  int64_t token = 0;
  if (optionType.toUpper() == "CE") {
    token = tokens.first;   // Call token
  } else if (optionType.toUpper() == "PE") {
    token = tokens.second;  // Put token
  }
  
  if (token == 0) {
    qWarning() << "[OptionsEngine] Token not found for"
               << symbol << expiry << strike << optionType;
  } else {
    qDebug() << "[OptionsEngine] Found token:" << token
             << "for" << symbol << strike << optionType;
  }
  
  return token;
}

// ═══════════════════════════════════════════════════════════
// Expiry Resolution (Simplified for POC)
// ═══════════════════════════════════════════════════════════

QString OptionsExecutionEngine::resolveCurrentWeeklyExpiry(const QString& symbol) {
  Q_UNUSED(symbol);
  
  // POC: Hardcoded nearest weekly expiry
  // Production: Query RepositoryManager::getExpiriesForSymbol() and find nearest
  
  QDate today = QDate::currentDate();
  
  // Find next Thursday
  int daysToThursday = (Qt::Thursday - today.dayOfWeek() + 7) % 7;
  if (daysToThursday == 0) {
    daysToThursday = 7;  // If today is Thursday, use next week
  }
  
  QDate expiryDate = today.addDays(daysToThursday);
  
  // Format: "30JAN26" (DDMMMYY uppercase)
  QString expiry = expiryDate.toString("ddMMMyy").toUpper();
  
  qDebug() << "[OptionsEngine] Resolved weekly expiry:" << expiry
           << "for" << symbol;
  
  return expiry;
}

// ═══════════════════════════════════════════════════════════
// Leg Resolution (Main Entry Point)
// ═══════════════════════════════════════════════════════════

OptionsExecutionEngine::ResolvedLeg 
OptionsExecutionEngine::resolveLeg(const OptionLeg& leg,
                                   const QString& strategySymbol,
                                   double spotPrice) {
  ResolvedLeg resolved;
  resolved.legId = leg.legId;
  resolved.side = leg.side;
  resolved.optionType = leg.optionType;
  resolved.quantity = leg.quantity;
  resolved.valid = false;
  
  // Step 1: Determine symbol (use leg.symbol if provided, else strategy symbol)
  QString symbol = leg.symbol.isEmpty() ? strategySymbol : leg.symbol;
  
  if (symbol.isEmpty()) {
    resolved.errorMsg = "No symbol specified for leg";
    qCritical() << "[OptionsEngine]" << resolved.errorMsg;
    return resolved;
  }
  
  // Step 2: Resolve expiry
  QString expiry;
  if (leg.expiry == ExpiryType::CurrentWeekly) {
    expiry = resolveCurrentWeeklyExpiry(symbol);
  } else if (leg.expiry == ExpiryType::SpecificDate) {
    expiry = leg.specificExpiry;
  } else {
    // For POC, only CurrentWeekly is fully implemented
    resolved.errorMsg = "Expiry type not yet supported in POC";
    qWarning() << "[OptionsEngine]" << resolved.errorMsg;
    return resolved;
  }
  
  // Step 3: Resolve strike
  int strike = 0;
  if (leg.strikeMode == StrikeSelectionMode::ATMRelative) {
    strike = resolveATMStrike(symbol, expiry, spotPrice, leg.atmOffset);
  } else if (leg.strikeMode == StrikeSelectionMode::FixedStrike) {
    strike = leg.fixedStrike;
  } else {
    resolved.errorMsg = "PremiumBased strike mode not yet implemented";
    qWarning() << "[OptionsEngine]" << resolved.errorMsg;
    return resolved;
  }
  
  if (strike == 0) {
    resolved.errorMsg = "Strike resolution failed";
    qCritical() << "[OptionsEngine]" << resolved.errorMsg;
    return resolved;
  }
  
  resolved.strike = strike;
  
  // Step 4: Build trading symbol
  resolved.tradingSymbol = buildOptionSymbol(symbol, strike, leg.optionType, expiry);
  
  // Step 5: Get contract token
  resolved.token = getContractToken(symbol, expiry, strike, leg.optionType);
  
  if (resolved.token == 0) {
    resolved.errorMsg = QString("Contract not found: %1").arg(resolved.tradingSymbol);
    qCritical() << "[OptionsEngine]" << resolved.errorMsg;
    return resolved;
  }
  
  // Success!
  resolved.valid = true;
  
  qInfo() << "[OptionsEngine] ✅ Leg resolved:"
          << resolved.legId << "→" << resolved.tradingSymbol
          << "strike:" << resolved.strike
          << "token:" << resolved.token;
  
  return resolved;
}

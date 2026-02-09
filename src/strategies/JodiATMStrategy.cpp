#include "strategies/JodiATMStrategy.h"
#include "services/ATMWatchManager.h"
#include "services/FeedHandler.h"
#include <QDebug>
#include <cmath>

JodiATMStrategy::JodiATMStrategy(QObject *parent) : StrategyBase(parent) {}

void JodiATMStrategy::init(const StrategyInstance &instance) {
  StrategyBase::init(instance);

  m_offset = getParameter<double>("offset", 10.0);
  m_threshold = getParameter<double>("threshold", 15.0);
  m_adjPts = getParameter<double>("adj_pts", 0.0);
  m_baseQty = instance.quantity;

  // New parameters from UI reference
  double diffPoints = getParameter<double>("diff_points", 0.0);
  bool isTrailing = getParameter<bool>("is_trailing", false);

  log(QString("Initialized JodiATM | Offset:%1, Threshold:%2, AdjPts:%3, "
              "Trailing:%4")
          .arg(m_offset)
          .arg(m_threshold)
          .arg(m_adjPts)
          .arg(isTrailing ? "On" : "Off"));
}

void JodiATMStrategy::start() {
  log("Starting Jodi-ATM strategy...");

  auto &mgr = ATMWatchManager::getInstance();
  auto info = mgr.getATMInfo(m_instance.symbol);

  if (!info.isValid) {
    log("Waiting for valid ATM info to start...");
    connect(&mgr, &ATMWatchManager::atmUpdated, this,
            &JodiATMStrategy::onATMUpdated);
    return;
  }

  resetATM(info.atmStrike);

  // Subscribe to Cash/Future for triggers
  m_cashToken = info.underlyingToken;
  FeedHandler::instance().subscribe(m_instance.segment, m_cashToken, this,
                                    &JodiATMStrategy::onTick);

  m_isRunning = true;
  updateState(StrategyState::Running);

  // Connect for future ATM shifts (major moves)
  connect(&mgr, &ATMWatchManager::atmUpdated, this,
          &JodiATMStrategy::onATMUpdated);
}

void JodiATMStrategy::stop() {
  log("Stopping Jodi-ATM strategy...");
  m_isRunning = false;
  disconnect(&ATMWatchManager::getInstance(), &ATMWatchManager::atmUpdated,
             this, &JodiATMStrategy::onATMUpdated);
  unsubscribe();
  updateState(StrategyState::Stopped);
}

void JodiATMStrategy::onATMUpdated() {
  auto info = ATMWatchManager::getInstance().getATMInfo(m_instance.symbol);
  if (!info.isValid)
    return;

  // The mgr ATM updated. Check if we have moved a full strike.
  // In the reference code, resetATM (getNewData) is called when self.blDP ==
  // self.BLRCP etc. We'll handle major ATM shifts here if the manager's ATM
  // deviates from our current ATM.
  if (std::abs(info.atmStrike - m_currentATM) >= m_strikeDiff &&
      m_strikeDiff > 0) {
    log(QString(
            "Major ATM Shift detected: %1 -> %2. Resetting strategy bounds.")
            .arg(m_currentATM)
            .arg(info.atmStrike));
    resetATM(info.atmStrike);
  }
}

void JodiATMStrategy::resetATM(double newATM) {
  m_currentATM = newATM;

  auto info = ATMWatchManager::getInstance().getATMInfo(m_instance.symbol);
  if (info.strikes.size() >= 2) {
    m_strikeDiff = std::abs(info.strikes[1] - info.strikes[0]);
  } else {
    m_strikeDiff = 100.0; // Default fallback
  }

  m_ceToken = info.callToken;
  m_peToken = info.putToken;

  // Calculate RCPs (Reset Constant Points) - usually +- 1.6 strike diff from
  // reference
  m_blRCP = m_currentATM + (1.6 * m_strikeDiff) + m_offset - m_adjPts;
  m_brRCP = m_currentATM - (1.6 * m_strikeDiff) - m_offset - m_adjPts;

  m_currentLeg = 0;
  m_trend = Trend::Nutral;
  m_isFirstOrderPlaced = false;

  calculateThresholds(m_currentATM);

  log(QString("ATM Reset to %1. Bounds: [%2, %3]. RCPs: [%4, %5]")
          .arg(m_currentATM)
          .arg(m_brDP)
          .arg(m_blDP)
          .arg(m_brRCP)
          .arg(m_blRCP));
}

void JodiATMStrategy::calculateThresholds(double atm) {
  double refATM = atm - m_adjPts;

  // Logic from getFSlDict / getSlDict
  // Leg 1 thresholds:
  // Bullish: ATM + offset + threshold + 0.6*diff
  // Bearish: ATM - offset - threshold - 0.6*diff

  if (m_currentLeg < 4) {
    double strikeMultiplier = 0.6 + (m_currentLeg * 0.1);
    double revMultiplier = 0.1 + (m_currentLeg * 0.1);

    m_blDP =
        refATM + m_offset + m_threshold + (strikeMultiplier * m_strikeDiff);
    m_brDP =
        refATM - m_offset - m_threshold - (strikeMultiplier * m_strikeDiff);

    if (m_currentLeg > 0) {
      m_reversalP = (m_trend == Trend::Bullish)
                        ? (refATM + m_offset + (revMultiplier * m_strikeDiff))
                        : (refATM - m_offset - (revMultiplier * m_strikeDiff));
    } else {
      m_reversalP = 0.0; // No reversal in neutral
    }
  } else {
    m_blDP = m_blRCP;
    m_brDP = m_brRCP;
  }
}

void JodiATMStrategy::onTick(const UDP::MarketTick &tick) {
  if (!m_isRunning)
    return;

  if (tick.token == m_cashToken) {
    m_cashPrice = tick.ltp;
    checkTrade(m_cashPrice);
  }
}

void JodiATMStrategy::checkTrade(double cashPrice) {
  if (!m_isRunning)
    return;

  // First order entry
  if (!m_isFirstOrderPlaced && m_trend == Trend::Nutral) {
    log(QString("First Entry: Selling Jodi at %1").arg(m_currentATM));
    // makeOrder(m_ceToken, m_baseQty, "Sell");
    // makeOrder(m_peToken, m_baseQty, "Sell");
    m_isFirstOrderPlaced = true;
  }

  if (m_trend == Trend::Nutral) {
    if (cashPrice > m_blDP) {
      m_trend = Trend::Bullish;
      m_currentLeg = 1;
      log("Trend Change: BULLISH. Leg 1 triggered.");
      calculateThresholds(m_currentATM);
      // switchCurrent2Upper(); // Shift 25%
    } else if (cashPrice < m_brDP) {
      m_trend = Trend::Bearish;
      m_currentLeg = 1;
      log("Trend Change: BEARISH. Leg 1 triggered.");
      calculateThresholds(m_currentATM);
      // switchCurrent2Lower();
    }
  } else if (m_trend == Trend::Bullish) {
    if (cashPrice > m_blDP) {
      if (m_blDP >= m_blRCP) {
        log("Bullish Hit RCP. Shifting ATM Up.");
        resetATM(m_currentATM + m_strikeDiff);
      } else {
        m_currentLeg++;
        log(QString("Bullish Leg Extension: Leg %1").arg(m_currentLeg));
        calculateThresholds(m_currentATM);
        // shiftQuantity(...)
      }
    } else if (m_reversalP > 0 && cashPrice < m_reversalP) {
      m_currentLeg--;
      if (m_currentLeg == 0)
        m_trend = Trend::Nutral;
      log(QString("Bullish Reversal: Returning to Leg %1").arg(m_currentLeg));
      calculateThresholds(m_currentATM);
      // reverseShift(...)
    }
  } else if (m_trend == Trend::Bearish) {
    if (cashPrice < m_brDP) {
      if (m_brDP <= m_brRCP) {
        log("Bearish Hit RCP. Shifting ATM Down.");
        resetATM(m_currentATM - m_strikeDiff);
      } else {
        m_currentLeg++;
        log(QString("Bearish Leg Extension: Leg %1").arg(m_currentLeg));
        calculateThresholds(m_currentATM);
      }
    } else if (m_reversalP > 0 && cashPrice > m_reversalP) {
      m_currentLeg--;
      if (m_currentLeg == 0)
        m_trend = Trend::Nutral;
      log(QString("Bearish Reversal: Returning to Leg %1").arg(m_currentLeg));
      calculateThresholds(m_currentATM);
    }
  }
}

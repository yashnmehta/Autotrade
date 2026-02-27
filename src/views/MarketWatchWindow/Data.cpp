#include "models/domain/TokenAddressBook.h"
#include "repository/RepositoryManager.h"
#include "utils/LatencyTracker.h"
#include "views/MarketWatchWindow.h"
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <iostream>


void MarketWatchWindow::updatePrice(int token, double ltp, double change,
                                    double changePercent) {
  QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
  for (int row : rows) {
    m_model->updatePrice(row, ltp, change, changePercent);
  }
}

void MarketWatchWindow::updateVolume(int token, qint64 volume) {
  QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
  for (int row : rows) {
    m_model->updateVolume(row, volume);
  }
}

void MarketWatchWindow::updateBidAsk(int token, double bid, double ask) {
  QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
  for (int row : rows) {
    m_model->updateBidAsk(row, bid, ask);
  }
}

void MarketWatchWindow::updateOHLC(int token, double open, double high,
                                   double low, double close) {
  QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
  for (int row : rows) {
    m_model->updateOHLC(row, open, high, low, close);
  }
}

void MarketWatchWindow::updateBidAskQuantities(int token, int bidQty,
                                               int askQty) {
  QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
  for (int row : rows) {
    m_model->updateBidAskQuantities(row, bidQty, askQty);
  }
}

void MarketWatchWindow::updateTotalBuySellQty(int token, int totalBuyQty,
                                              int totalSellQty) {
  QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
  for (int row : rows) {
    m_model->updateTotalBuySellQty(row, totalBuyQty, totalSellQty);
  }
}

void MarketWatchWindow::updateOpenInterest(int token, qint64 oi,
                                           double oiChangePercent) {
  QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
  for (int row : rows) {
    m_model->updateOpenInterestWithChange(row, oi, oiChangePercent);
  }
}

void MarketWatchWindow::updateLastTradedQuantity(int token, qint64 ltq) {
  QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
  for (int row : rows) {
    m_model->updateLastTradedQuantity(row, ltq);
  }
}

void MarketWatchWindow::onTickUpdate(const XTS::Tick &tick) {
  int token = (int)tick.exchangeInstrumentID;
  int64_t timestampModelStart = LatencyTracker::now();

  // Use optimized int64 composite key lookup
  QList<int> rows =
      m_tokenAddressBook->getRowsForIntKey(tick.exchangeSegment, token);

  if (rows.isEmpty())
    return;

  // Debug logging for BSE tokens AND bid/ask data
  if (tick.exchangeSegment == 12 || tick.exchangeSegment == 11) {
    static int bseUpdateCount = 0;
    if (bseUpdateCount++ < 10) {
      // int row = rows.first();
      // qDebug() << "[MarketWatch] BSE Tick Update - Token:" << token
      //          << "LTP:" << tick.lastTradedPrice
      //          << "Bid:" << tick.bidPrice << "BidQty:" << tick.bidQuantity
      //          << "Ask:" << tick.askPrice << "AskQty:" << tick.askQuantity
      //          << "Row:" << row;
    }
  }

  // Debug: Log all bid/ask updates for first 100 ticks (extended for diagnosis)
  static int bidAskDebugCount = 0;
  if (bidAskDebugCount++ < 100) {
    // qDebug() << "[MarketWatch] Tick" << bidAskDebugCount << "Token:" << token
    //          << "Segment:" << tick.exchangeSegment
    //          << "Bid:" << tick.bidPrice << "(" << tick.bidQuantity << ")"
    //          << "Ask:" << tick.askPrice << "(" << tick.askQuantity << ")";
  }

  // 1. Update LTP and OHLC if LTP is present (> 0)
  if (tick.lastTradedPrice > 0) {
    double change = 0;
    double changePercent = 0;

    // Find stored close price if tick doesn't have it
    double closePrice = tick.close;
    if (closePrice <= 0) {
      // Use the first row to get the close price (assuming close is same for
      // all duplicates of same exchange contract)
      closePrice = m_model->getScripAt(rows.first()).close;
    }

    if (closePrice > 0) {
      change = tick.lastTradedPrice - closePrice;
      changePercent = (change / closePrice) * 100.0;
    }

    for (int row : rows) {
      m_model->updatePrice(row, tick.lastTradedPrice, change, changePercent);
    }

    if (tick.open > 0 || tick.high > 0 || tick.low > 0 || tick.close > 0) {
      for (int row : rows) {
        m_model->updateOHLC(row, tick.open, tick.high, tick.low, tick.close);
      }
    }
  }

  // 2. Update LTQ if present
  if (tick.lastTradedQuantity > 0) {
    for (int row : rows) {
      m_model->updateLastTradedQuantity(row, tick.lastTradedQuantity);
    }
  }

  // 3. Update Average Price if provided
  if (tick.averagePrice > 0) {
    for (int row : rows) {
      m_model->updateAveragePrice(row, tick.averagePrice);
    }
  }

  // 4. Update Volume if provided
  if (tick.volume > 0) {
    for (int row : rows) {
      m_model->updateVolume(row, tick.volume);
    }
  }

  // 5. Update Bid/Ask and Market Depth
  if (tick.bidPrice > 0 || tick.askPrice > 0) {
    for (int row : rows) {
      m_model->updateBidAsk(row, tick.bidPrice, tick.askPrice);
      m_model->updateBidAskQuantities(row, tick.bidQuantity, tick.askQuantity);
    }
  }

  // 6. Update Total Buy/Sell Qty
  if (tick.totalBuyQuantity > 0 || tick.totalSellQuantity > 0) {
    for (int row : rows) {
      m_model->updateTotalBuySellQty(row, tick.totalBuyQuantity,
                                     tick.totalSellQuantity);
    }
  }

  // 7. Update Open Interest
  if (tick.openInterest > 0) {
    for (int row : rows) {
      m_model->updateOpenInterestWithChange(row, tick.openInterest, 0.0);
    }
  }

  int64_t timestampModelEnd = LatencyTracker::now();

  if (tick.refNo > 0 && tick.timestampUdpRecv > 0) {
    LatencyTracker::recordLatency(tick.timestampUdpRecv, tick.timestampParsed,
                                  tick.timestampQueued, tick.timestampDequeued,
                                  tick.timestampFeedHandler,
                                  timestampModelStart, timestampModelEnd);
  }
}

// NEW: UDP-specific tick handler with cleaner semantics
// NEW: UDP-specific tick handler with cleaner semantics
void MarketWatchWindow::onUdpTickUpdate(const UDP::MarketTick &tick) {
  // ⏱️ PERFORMANCE LOG: Start timing tick update processing
  static int tickCounter = 0;
  static int detailedLogCounter = 0;
  tickCounter++;
  bool shouldLogDetailed =
      (detailedLogCounter < 50); // Log first 50 ticks in detail

  QElapsedTimer tickTimer;
  if (shouldLogDetailed) {
    tickTimer.start();
  }

  int token = tick.token;
  int64_t timestampModelStart = LatencyTracker::now();

  /* if (shouldLogDetailed) {
      detailedLogCounter++;
      qDebug() << "[PERF] [MW_TICK_UPDATE] #" << tickCounter << "START - Token:"
  << token
               << "Segment:" << static_cast<int>(tick.exchangeSegment)
               << "LTP:" << tick.ltp;
  } */

  qint64 t0 = shouldLogDetailed ? tickTimer.elapsed() : 0;

  // Use optimized int64 composite key lookup
  QList<int> rows = m_tokenAddressBook->getRowsForIntKey(
      static_cast<int>(tick.exchangeSegment), token);

  qint64 t1 = shouldLogDetailed ? tickTimer.elapsed() : 0;

  if (rows.isEmpty()) {
    /* if (shouldLogDetailed) {
        qDebug() << "[PERF] [MW_TICK_UPDATE] #" << tickCounter << "NO ROWS -
    Token:" << token;
    } */
    return;
  }

  /* if (shouldLogDetailed) {
      qDebug() << "[PERF] [MW_TICK_UPDATE] #" << tickCounter << "Found" <<
  rows.size() << "row(s)";
  } */

  // Debug logging for BSE tokens
  if (tick.exchangeSegment == UDP::ExchangeSegment::BSEFO ||
      tick.exchangeSegment == UDP::ExchangeSegment::BSECM) {
    static int bseUpdateCount = 0;
    if (bseUpdateCount++ < 10) {
      // int row = rows.first();
      // qDebug() << "[MarketWatch] BSE UDP Tick - Token:" << token
      //          << "LTP:" << tick.ltp
      //          << "Bid:" << tick.bestBid() << "(" << tick.bids[0].quantity <<
      //          ")"
      //          << "Ask:" << tick.bestAsk() << "(" << tick.asks[0].quantity <<
      //          ")"
      //          << "Row:" << row;
    }
  }

  qint64 t2 = shouldLogDetailed ? tickTimer.elapsed() : 0;

  // 1. Update LTP and OHLC if LTP is present
  if (tick.ltp > 0) {
    double change = 0;
    double changePercent = 0;

    // Use prevClose from UDP tick (explicit semantic)
    double closePrice = tick.prevClose;
    if (closePrice <= 0) {
      // Use the first row to get the close price
      closePrice = m_model->getScripAt(rows.first()).close;
    }

    if (closePrice > 0) {
      change = tick.ltp - closePrice;
      changePercent = (change / closePrice) * 100.0;
    }

    for (int row : rows) {
      m_model->updatePrice(row, tick.ltp, change, changePercent);
    }

    // Update OHLC - prevClose is explicit in UDP
    if (tick.open > 0 || tick.high > 0 || tick.low > 0 || tick.prevClose > 0) {
      for (int row : rows) {
        m_model->updateOHLC(row, tick.open, tick.high, tick.low,
                            tick.prevClose);
      }
    }
  }

  qint64 t3 = shouldLogDetailed ? tickTimer.elapsed() : 0;

  // 2. Update LTQ if present
  if (tick.ltq > 0) {
    for (int row : rows) {
      m_model->updateLastTradedQuantity(row, tick.ltq);
    }
  }

  // 3. Update Average Traded Price (ATP - explicit naming in UDP)
  if (tick.atp > 0) {
    for (int row : rows) {
      m_model->updateAveragePrice(row, tick.atp);
    }
  }

  // 4. Update Volume if provided
  if (tick.volume > 0) {
    for (int row : rows) {
      m_model->updateVolume(row, tick.volume);
    }
  }

  qint64 t4 = shouldLogDetailed ? tickTimer.elapsed() : 0;

  // 5. Update Bid/Ask from 5-level depth (best bid/ask from level 0)
  if (tick.bids[0].price > 0 || tick.asks[0].price > 0) {
    for (int row : rows) {
      m_model->updateBidAsk(row, tick.bids[0].price, tick.asks[0].price);
      m_model->updateBidAskQuantities(row, (int)tick.bids[0].quantity,
                                      (int)tick.asks[0].quantity);
    }
  }

  qint64 t5 = shouldLogDetailed ? tickTimer.elapsed() : 0;

  // 6. Update Total Buy/Sell Qty (aggregated from 5 levels)
  if (tick.totalBidQty > 0 || tick.totalAskQty > 0) {
    for (int row : rows) {
      m_model->updateTotalBuySellQty(row, (int)tick.totalBidQty,
                                     (int)tick.totalAskQty);
    }
  }

  // 7. Update Open Interest (for derivatives only)
  if (tick.isDerivative() && tick.openInterest > 0) {
    double oiChangePercent = 0.0;
    if (tick.oiChange != 0 && tick.openInterest > 0) {
      oiChangePercent =
          (static_cast<double>(tick.oiChange) / tick.openInterest) * 100.0;
    }
    for (int row : rows) {
      m_model->updateOpenInterestWithChange(row, tick.openInterest,
                                            oiChangePercent);
    }
  }

  qint64 t6 = shouldLogDetailed ? tickTimer.elapsed() : 0;

  int64_t timestampModelEnd = LatencyTracker::now();

  qint64 totalTime = shouldLogDetailed ? tickTimer.elapsed() : 0;

  /* if (shouldLogDetailed) {
      qDebug() << "[PERF] [MW_TICK_UPDATE] #" << tickCounter << "COMPLETE -
  Token:" << token; qDebug() << "  TOTAL TIME:" << totalTime << "ms"; qDebug()
  << "  Breakdown:"; qDebug() << "    - Initial setup:" << t0 << "ms"; qDebug()
  << "    - Row lookup:" << (t1-t0) << "ms"; qDebug() << "    - BSE logging
  check:" << (t2-t1) << "ms"; qDebug() << "    - LTP + OHLC update:" << (t3-t2)
  << "ms"; qDebug() << "    - LTQ/ATP/Volume update:" << (t4-t3) << "ms";
      qDebug() << "    - Bid/Ask update:" << (t5-t4) << "ms";
      qDebug() << "    - Total Qty + OI update:" << (t6-t5) << "ms";
  } */

  // Latency tracking (every 100th tick for summary stats)
  if (tick.refNo > 0 && tick.timestampUdpRecv > 0) {
    LatencyTracker::recordLatency(
        tick.timestampUdpRecv, tick.timestampParsed, tick.timestampEmitted,
        0, // No dequeue timestamp for UDP path
        tick.timestampFeedHandler, timestampModelStart, timestampModelEnd);
  }

  // Log summary every 100 ticks
  /* if (tickCounter % 100 == 0) {
      qDebug() << "[PERF] [MW_TICK_UPDATE] Processed" << tickCounter << "ticks
  total";
  } */
}

int MarketWatchWindow::findTokenRow(int token) const {
  QList<int> rows = m_tokenAddressBook->getRowsForToken(token);
  return rows.isEmpty() ? -1 : rows.first();
}

bool MarketWatchWindow::hasDuplicate(int token) const {
  return m_tokenAddressBook->hasToken(token);
}

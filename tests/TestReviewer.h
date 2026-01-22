#ifndef TEST_REVIEWER_H
#define TEST_REVIEWER_H

#include "services/UdpBroadcastService.h"
#include <QCoreApplication>
#include <QObject>
#include <QTimer>
#include <cstring>
#include <iostream>

class TestReviewer : public QObject {
  Q_OBJECT
public:
  bool nseCmReceived = false;
  bool nseFoReceived = false;

public slots:
  void onIndexReceived(const UDP::IndexTick &tick) {
    QString name = QString::fromUtf8(tick.name).trimmed();
    std::cout << "Test: Received Index Tick Segment: "
              << (int)tick.exchangeSegment << " Name: " << name.toStdString()
              << " Value: " << tick.value << std::endl;

    // Check for ANY Nifty variant
    bool matchesNifty = (name.contains("Nifty", Qt::CaseInsensitive));

    if (matchesNifty) {
      std::cout << ">>> FOUND NIFTY VARIANT: " << name.toStdString()
                << " on segment " << (int)tick.exchangeSegment << std::endl;

      // Specifically look for the primary Nifty 50
      bool isNifty50 = (name == "Nifty 50" || name == "NIFTY 50" ||
                        name == "NIFTY50" || name == "Nifty50");

      if (isNifty50) {
        if (tick.exchangeSegment == UDP::ExchangeSegment::NSECM) {
          nseCmReceived = true;
          std::cout << "  -> Verified NSECM Nifty 50 (" << name.toStdString()
                    << ")" << std::endl;
        } else if (tick.exchangeSegment == UDP::ExchangeSegment::NSEFO) {
          nseFoReceived = true;
          std::cout << "  -> Verified NSEFO Nifty 50 (" << name.toStdString()
                    << ")" << std::endl;
        }
      }
    }

    if (nseCmReceived && nseFoReceived) {
      std::cout << "SUCCESS: Both segments verified!" << std::endl;
      QTimer::singleShot(2000, qApp, &QCoreApplication::quit);
    }
  }

  void onTickReceived(const UDP::MarketTick &tick) {
    if (tick.token == 26000) {
      std::cout << "Test: Received Ticker Tick for Token 26000 Segment: "
                << (int)tick.exchangeSegment << " LTP: " << tick.ltp
                << std::endl;
    }
  }
};

#endif // TEST_REVIEWER_H

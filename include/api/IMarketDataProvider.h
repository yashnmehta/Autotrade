#ifndef I_MARKET_DATA_PROVIDER_H
#define I_MARKET_DATA_PROVIDER_H

#include <vector>
#include <functional>
#include <string>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QVector>

// Define Quote structure
struct Quote {
    int64_t token;
    int64_t timestamp;
    double ltp;
    int volume;
    double bidPrice;
    double askPrice;
    int exchangeSegment;
};

// Provider Types
enum class ProviderType {
    XTS,
    UDP,
    Hybrid
};

// Capabilities structure
struct ProviderCapabilities {
    bool supportsREST;
    bool supportsWebSocket;
    bool supportsUDP;
    std::vector<int> supportedExchanges;  // Exchange segments
    int averageLatencyMs;
};

class IMarketDataProvider {
public:
    using TickCallback = std::function<void(const Quote&)>;

    virtual ~IMarketDataProvider() = default;
    
    // Identification
    virtual ProviderType type() const = 0;
    virtual std::string name() const = 0;
    virtual ProviderCapabilities capabilities() const = 0;
    
    // Connection
    virtual void connect(const QJsonObject &config,
                        std::function<void(bool)> callback) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    
    // Subscription
    virtual void subscribe(const std::vector<int64_t> &tokens,
                          int exchangeSegment,
                          std::function<void(bool, const QJsonArray&)> callback) = 0;
    virtual void unsubscribe(const std::vector<int64_t> &tokens,
                            std::function<void(bool)> callback) = 0;
    
    // Quote retrieval
    virtual void getQuote(int64_t token,
                         int exchangeSegment,
                         std::function<void(bool, const Quote&)> callback) = 0;

    // Callback registration
    virtual void registerCallback(TickCallback callback) = 0;
};

#endif // I_MARKET_DATA_PROVIDER_H


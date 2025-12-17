#ifndef UDP_BROADCAST_PROVIDER_H
#define UDP_BROADCAST_PROVIDER_H

#include <vector>
#include <string>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include "IMarketDataProvider.h"

// Forward declaration
struct sockaddr_in;

class UDPBroadcastProvider : public IMarketDataProvider {
public:
    UDPBroadcastProvider();
    ~UDPBroadcastProvider() override;
    
    // IMarketDataProvider implementation
    ProviderType type() const override { return ProviderType::UDP; }
    std::string name() const override { return "Exchange UDP Broadcast"; }
    ProviderCapabilities capabilities() const override;
    
    void connect(const QJsonObject &config,
                std::function<void(bool)> callback) override;
    void disconnect() override;
    bool isConnected() const override { return m_connected; }
    
    void subscribe(const std::vector<int64_t> &tokens, int exchangeSegment,
                  std::function<void(bool, const QJsonArray&)> callback) override;
    void unsubscribe(const std::vector<int64_t> &tokens,
                    std::function<void(bool)> callback) override;
    
    void getQuote(int64_t token, int exchangeSegment,
                 std::function<void(bool, const Quote&)> callback) override;
    
    void registerCallback(TickCallback callback) override;

private:
    void readLoop();
    void processPacket(const std::vector<uint8_t>& packet, const std::string& sourceIp);

    // Configuration per exchange
    struct ExchangeConfig {
        bool enabled;
        std::string multicastGroup;
        uint16_t port;
        std::string protocol;
    };
    
    std::map<int, ExchangeConfig> m_exchangeConfigs; 
    std::map<int, int> m_sockets; // Segment -> Socket FD
    std::unordered_set<int64_t> m_subscribedTokens;
    std::unordered_map<int64_t, int> m_tokenToExchange;
    
    std::atomic<bool> m_connected;
    std::atomic<bool> m_running;
    std::thread m_readThread;
    
    TickCallback m_callback;
    std::mutex m_callbackMutex;
    std::mutex m_subscriptionMutex;

    // Parsing
    Quote parseNSEPacket(const uint8_t* data, size_t size);
    Quote parseBSEPacket(const uint8_t* data, size_t size);
    
    // Native Socket helpers
    bool initUDPSocket(int exchangeSegment, const ExchangeConfig &config);
    void closeSockets();
};

#endif // UDP_BROADCAST_PROVIDER_H

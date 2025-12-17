#include "api/UDPBroadcastProvider.h"
#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include "api/NSEProtocol.h"

UDPBroadcastProvider::UDPBroadcastProvider() 
    : m_connected(false), m_running(false) {
}

UDPBroadcastProvider::~UDPBroadcastProvider() {
    disconnect();
}

ProviderCapabilities UDPBroadcastProvider::capabilities() const {
    ProviderCapabilities caps;
    caps.supportsREST = false;
    caps.supportsWebSocket = false;
    caps.supportsUDP = true;
    caps.supportedExchanges = {1, 2, 13, 11, 12, 61};
    caps.averageLatencyMs = 1;
    return caps;
}

void UDPBroadcastProvider::connect(const QJsonObject &config,
                                   std::function<void(bool)> callback) {
    std::cout << "[UDPBroadcast] Initializing..." << std::endl;
    
    QJsonObject udpConfig = config["udp"].toObject();
    QJsonObject exchanges = udpConfig["exchanges"].toObject();
    bool anyConnected = false;
    
    // Setup NSE CM
    if (exchanges.contains("NSECM")) {
        QJsonObject nseConfig = exchanges["NSECM"].toObject();
        if (nseConfig["enabled"].toBool()) {
            ExchangeConfig cfg;
            cfg.enabled = true;
            cfg.multicastGroup = nseConfig["multicastGroup"].toString().toStdString();
            cfg.port = static_cast<uint16_t>(nseConfig["port"].toInt());
            cfg.protocol = nseConfig["protocol"].toString().toStdString();
            
            m_exchangeConfigs[1] = cfg;  // NSECM
            
            if (initUDPSocket(1, cfg)) {
                 anyConnected = true;
                 m_sockets[13] = m_sockets[1]; // NSECD share with CM if not specified? 
                 // Actually NSECD might use a different port too, but user only gave NSECM and NSEFO.
                 // Let's assume CD uses the same as CM for now or not connected if not specified.
                 // But previously I mapped 13->1. Let's keep 13->1 for continuity unless shown otherwise.
            }
        }
    }

    // Setup NSE FO
    if (exchanges.contains("NSEFO")) {
        QJsonObject nseConfig = exchanges["NSEFO"].toObject();
        if (nseConfig["enabled"].toBool()) {
            ExchangeConfig cfg;
            cfg.enabled = true;
            cfg.multicastGroup = nseConfig["multicastGroup"].toString().toStdString();
            cfg.port = static_cast<uint16_t>(nseConfig["port"].toInt());
            cfg.protocol = nseConfig["protocol"].toString().toStdString();
            
            m_exchangeConfigs[2] = cfg;  // NSEFO
            
            if (initUDPSocket(2, cfg)) {
                 anyConnected = true;
                 // NSE FO specific logic
            }
        }
    }
    
    // Setup BSE CM
    if (exchanges.contains("BSECM")) {
        QJsonObject bseConfig = exchanges["BSECM"].toObject();
        if (bseConfig["enabled"].toBool()) {
            ExchangeConfig cfg;
            cfg.enabled = true;
            cfg.multicastGroup = bseConfig["multicastGroup"].toString().toStdString();
            cfg.port = static_cast<uint16_t>(bseConfig["port"].toInt());
            cfg.protocol = bseConfig["protocol"].toString().toStdString();
            
            m_exchangeConfigs[11] = cfg;  // BSECM
            
            if (initUDPSocket(11, cfg)) {
                anyConnected = true;
                m_sockets[61] = m_sockets[11]; // BSECD shared
            }
        }
    }

    // Setup BSE FO
    if (exchanges.contains("BSEFO")) {
        QJsonObject bseConfig = exchanges["BSEFO"].toObject();
        if (bseConfig["enabled"].toBool()) {
            ExchangeConfig cfg;
            cfg.enabled = true;
            cfg.multicastGroup = bseConfig["multicastGroup"].toString().toStdString();
            cfg.port = static_cast<uint16_t>(bseConfig["port"].toInt());
            cfg.protocol = bseConfig["protocol"].toString().toStdString();
            
            m_exchangeConfigs[12] = cfg;  // BSEFO
            
            if (initUDPSocket(12, cfg)) {
                anyConnected = true;
            }
        }
    }
    
    m_connected = anyConnected;
    if (m_connected) {
        m_running = true;
        m_readThread = std::thread(&UDPBroadcastProvider::readLoop, this);
    }
    
    std::cout << "[UDPBroadcast] Connected status: " << m_connected << std::endl;
    if (callback) callback(m_connected);
}

void UDPBroadcastProvider::disconnect() {
    m_running = false;
    if (m_readThread.joinable()) {
        m_readThread.join();
    }
    closeSockets();
    m_connected = false;
}

void UDPBroadcastProvider::closeSockets() {
    for (auto const& [segment, fd] : m_sockets) {
        // Only close unique FDs (avoid double close since we map multiple segments to same FD)
        // Simple check: create a set of FDs to close
    }
    std::unordered_set<int> uniqueFDs;
    for (auto const& [segment, fd] : m_sockets) {
        if (fd > 0) uniqueFDs.insert(fd);
    }
    for (int fd : uniqueFDs) {
        ::close(fd);
    }
    m_sockets.clear();
}

bool UDPBroadcastProvider::initUDPSocket(int exchangeSegment, const ExchangeConfig &config) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // Allow reusing address
    int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt(SO_REUSEADDR) failed" << std::endl;
        ::close(fd);
        return false;
    }
    
    #ifdef SO_REUSEPORT
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt(SO_REUSEPORT) failed" << std::endl;
        // Not fatal usually
    }
    #endif

    sockaddr_in localAddr;
    std::memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(config.port);

    if (bind(fd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        std::cerr << "Failed to bind to port " << config.port << std::endl;
        ::close(fd);
        return false;
    }

    // Join multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(config.multicastGroup.c_str());
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0) {
        std::cerr << "Failed to join multicast group " << config.multicastGroup << std::endl;
        ::close(fd);
        return false;
    }

    // Set non-blocking might be useful if we use select/poll, but separate thread with blocking read is also OK if we set a timeout
    // For now, let's keep it blocking with a timeout so we can check m_running
    struct timeval read_timeout;
    read_timeout.tv_sec = 1;
    read_timeout.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout));

    m_sockets[exchangeSegment] = fd;
    std::cout << "[UDPBroadcast] Listening on " << config.multicastGroup << ":" << config.port << " (FD: " << fd << ")" << std::endl;
    return true;
}

void UDPBroadcastProvider::readLoop() {
    std::vector<uint8_t> buffer(65536);
    
    while (m_running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = 0;
        
        // Add all unique sockets
        std::unordered_set<int> uniqueFDs;
        for (auto const& [seg, fd] : m_sockets) {
            if (fd > 0 && uniqueFDs.find(fd) == uniqueFDs.end()) {
                 FD_SET(fd, &readfds);
                 if (fd > max_fd) max_fd = fd;
                 uniqueFDs.insert(fd);
            }
        }
        
        if (max_fd == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int rv = select(max_fd + 1, &readfds, NULL, NULL, &tv);

        if (rv > 0) {
            for (int fd : uniqueFDs) {
                if (FD_ISSET(fd, &readfds)) {
                    sockaddr_in senderAddr;
                    socklen_t senderLen = sizeof(senderAddr);
                    ssize_t n = recvfrom(fd, buffer.data(), buffer.size(), 0, (struct sockaddr*)&senderAddr, &senderLen);
                    
                    if (n > 0) {
                        // Identify which segment this FD belongs to (first match is fine given we share FDs)
                        // Actually we might need to know the segment for parsing logic
                        int segment = -1;
                        for(auto const& [s, f] : m_sockets) {
                            if (f == fd) {
                                segment = s;
                                // If it's NSE group, use NSE parsing.
                                break;
                            }
                        }
                        
                        Quote quote;
                        if (segment >= 1 && segment <= 13) {
                            quote = parseNSEPacket(buffer.data(), n);
                        } else if (segment >= 11 && segment <= 61) {
                            quote = parseBSEPacket(buffer.data(), n);
                        }
                        
                        quote.exchangeSegment = segment;
                        
                        // Callback
                        std::lock_guard<std::mutex> lock(m_subscriptionMutex);
                        if (m_subscribedTokens.count(quote.token)) {
                             // invoke callback
                             std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                             if (m_callback) {
                                 m_callback(quote);
                             }
                        }
                    }
                }
            }
        }
    }
}

void UDPBroadcastProvider::subscribe(const std::vector<int64_t> &tokens,
                                     int exchangeSegment,
                                     std::function<void(bool, const QJsonArray&)> callback) {
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);
    for (int64_t token : tokens) {
        m_subscribedTokens.insert(token);
        m_tokenToExchange[token] = exchangeSegment;
    }
    if (callback) callback(true, QJsonArray());
}

void UDPBroadcastProvider::unsubscribe(const std::vector<int64_t> &tokens,
                                       std::function<void(bool)> callback) {
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);
    for (int64_t token : tokens) {
        m_subscribedTokens.erase(token);
        m_tokenToExchange.erase(token);
    }
    if (callback) callback(true);
}

void UDPBroadcastProvider::getQuote(int64_t token, int exchangeSegment,
                                    std::function<void(bool, const Quote&)> callback) {
    if (callback) {
        Quote empty = {0};
        callback(false, empty);
    }
}

void UDPBroadcastProvider::registerCallback(TickCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_callback = callback;
}

Quote UDPBroadcastProvider::parseNSEPacket(const uint8_t* data, size_t size) {
    Quote quote = {0};
    if (size < sizeof(Stream_Header) + 1) return quote;
    
    // Cast to Generic to check type
    const Generic_Message* msg = reinterpret_cast<const Generic_Message*>(data);
    
    // Check message length (sanity check)
    // Note: message_length in header might be size of this message or packet?
    // Usually it's size of this message.
    
    char msgType = msg->message_type;
    
    if (msgType == 'T') {
        if (size >= sizeof(Trade_Message)) {
            const Trade_Message* trade = reinterpret_cast<const Trade_Message*>(data);
            quote.token = trade->token;
            quote.timestamp = trade->timestamp;
            // Price is in paise (for CM/FO), so divide by 100. CD is 10^7.
            // Simplified logic: assume CM/FO (divide by 100)
            quote.ltp = trade->trade_price / 100.0;
            quote.volume = trade->trade_quantity; // Last traded qty
            // quote.totalVolume -> need accumulation, not available in single trade msg
            
            // Cannot determine Bid/Ask from Trade Message
        }
    } else if (msgType == 'N' || msgType == 'M' || msgType == 'X') {
        if (size >= sizeof(Order_Message)) {
            const Order_Message* order = reinterpret_cast<const Order_Message*>(data);
            quote.token = order->token;
            // Order message doesn't directly update LTP. 
            // It updates the Book.
            // For now, we return empty quote or we could expose order info differently.
            // But strict signature returns Quote.
            // We'll return a Quote marked as invalid or just minimal info for logging?
            // Actually, if we return Quote with ltp=0, the UI might show 0.
            // Let's filter these out in onReadyRead or just return token only?
            // "quote.token" is set. "quote.ltp" is 0.
        }
    }
    
    // We treat timestamp as epoch for now, though 8 bytes usually 1980 base.
    // If quote.token is 0, it means we failed to parse useful info.
    
    return quote;
}

Quote UDPBroadcastProvider::parseBSEPacket(const uint8_t* data, size_t size) {
    // Placeholder
    auto quote = parseNSEPacket(data, size);
    quote.exchangeSegment = 11;
    return quote;
}

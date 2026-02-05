/**
 * Native WebSocket Client Implementation
 * Based on Go reference:
 * trading_terminal_go/internal/services/xts/marketdata.go
 *
 * Uses Boost.Beast for WebSocket (zero Qt overhead)
 * Protocol: Engine.IO v3 + Socket.IO v2/v3
 */

#include "api/NativeWebSocketClient.h"
#include <algorithm>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <iostream>
#include <sstream>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

struct NativeWebSocketClient::Impl {
  net::io_context ioc;
  ssl::context sslCtx{ssl::context::tlsv12_client};
  tcp::resolver resolver{ioc};
  std::unique_ptr<websocket::stream<ssl::stream<tcp::socket>>>
      wss;                                            // SSL WebSocket
  std::unique_ptr<websocket::stream<tcp::socket>> ws; // Plain WebSocket

  std::thread ioThread;
  std::thread heartbeatThread;
  std::atomic<bool> running{false};

  beast::flat_buffer buffer;
  std::string currentUrl;
  std::string host;
  std::string port;
  std::string path;
  bool useSSL{true}; // Determined from URL scheme
};

NativeWebSocketClient::NativeWebSocketClient()
    : m_impl(std::make_unique<Impl>()) {
  // Configure SSL to ignore certificate errors (like Go implementation)
  m_impl->sslCtx.set_verify_mode(ssl::verify_none);
  m_impl->sslCtx.set_options(ssl::context::default_workarounds |
                             ssl::context::no_sslv2 | ssl::context::no_sslv3 |
                             ssl::context::tlsv12_client);
}

NativeWebSocketClient::~NativeWebSocketClient() { disconnect(); }

void NativeWebSocketClient::connect(const std::string &url,
                                    OnConnectedCallback onConnected,
                                    OnDisconnectedCallback onDisconnected,
                                    OnErrorCallback onError) {
  m_onConnected = onConnected;
  m_onDisconnected = onDisconnected;
  m_onError = onError;
  // If already connected or connecting, disconnect first
  disconnect();
  m_shouldReconnect = true;

  m_impl->running = true;
  m_impl->currentUrl = url;
  parseUrl(url);

  // Start I/O thread
  m_impl->ioThread = std::thread([this]() {
    try {
      // Resolve hostname
      auto const results = m_impl->resolver.resolve(m_impl->host, m_impl->port);

      if (m_impl->useSSL) {
        // Create SSL WebSocket stream
        m_impl->wss =
            std::make_unique<websocket::stream<ssl::stream<tcp::socket>>>(
                m_impl->ioc, m_impl->sslCtx);

        // Set SNI hostname
        if (!SSL_set_tlsext_host_name(m_impl->wss->next_layer().native_handle(),
                                      m_impl->host.c_str())) {
          throw std::runtime_error("Failed to set SNI hostname");
        }

        // Connect TCP socket
        net::connect(beast::get_lowest_layer(*m_impl->wss), results);

        // Perform SSL handshake
        m_impl->wss->next_layer().handshake(ssl::stream_base::client);

        // Set WebSocket options
        m_impl->wss->set_option(
            websocket::stream_base::decorator([](websocket::request_type &req) {
              req.set(http::field::user_agent, "TradingTerminal/1.0");
            }));

        // Perform WebSocket handshake
        m_impl->wss->handshake(m_impl->host, m_impl->path);
      } else {
        // Create plain WebSocket stream (no SSL)
        m_impl->ws =
            std::make_unique<websocket::stream<tcp::socket>>(m_impl->ioc);

        // Connect TCP socket
        net::connect(beast::get_lowest_layer(*m_impl->ws), results);

        // Set WebSocket options
        m_impl->ws->set_option(
            websocket::stream_base::decorator([](websocket::request_type &req) {
              req.set(http::field::user_agent, "TradingTerminal/1.0");
            }));

        // Perform WebSocket handshake
        m_impl->ws->handshake(m_impl->host, m_impl->path);
      }

      std::cout << "✅ Native WebSocket connected!" << std::endl;
      m_connected = true;
      m_reconnectAttempts = 0;

      if (m_onConnected) {
        m_onConnected();
      }

      // DO NOT send "40/" Socket.IO connect - Go implementation doesn't send
      // this! The Socket.IO namespace connection happens automatically via
      // Engine.IO handshake Sending "40/" causes duplicate "joined" events and
      // server disconnect

      // Start heartbeat
      startHeartbeat();

      // Read loop (like Go's for loop)
      while (m_impl->running && m_connected) {
        try {
          m_impl->buffer.clear();

          if (m_impl->useSSL) {
            m_impl->wss->read(m_impl->buffer);
          } else {
            m_impl->ws->read(m_impl->buffer);
          }

          std::string message = beast::buffers_to_string(m_impl->buffer.data());
          onMessage(message);

        } catch (beast::system_error const &se) {
          // Check if this is a clean close (error code 1)
          if (se.code() == websocket::error::closed) {
            std::cout << "⚠️ WebSocket closed by server" << std::endl;
            break; // Exit read loop cleanly
          }
          throw; // Re-throw other errors
        }
      }

    } catch (std::exception const &e) {
      std::cerr << "❌ Native WebSocket error: " << e.what() << std::endl;
      m_connected = false;

      if (m_onError) {
        m_onError(e.what());
      }
    }

    // Cleanup (outside try-catch to avoid nested exceptions)
    stopHeartbeat();

    // Notify disconnection callback
    if (m_onDisconnected) {
      m_onDisconnected("Connection closed");
    }

    // If we should reconnect, spawn a detached thread to trigger it
    // Note: Reconnect thread MUST NOT join the thread it was spawned from
    if (m_shouldReconnect && m_reconnectAttempts < 10) {
      std::thread([this]() { attemptReconnect(); }).detach();
    }
  });
}

void NativeWebSocketClient::disconnect() {
  m_shouldReconnect = false;
  m_impl->running = false;
  m_connected = false;

  stopHeartbeat();

  // Close the socket to break the read loop
  try {
    if (m_impl->useSSL && m_impl->wss && m_impl->wss->is_open()) {
      m_impl->wss->next_layer().next_layer().close();
    } else if (!m_impl->useSSL && m_impl->ws && m_impl->ws->is_open()) {
      m_impl->ws->next_layer().close();
    }
  } catch (...) {
  }

  if (m_impl->ioThread.joinable()) {
    // Only join if we are NOT the ioThread
    if (std::this_thread::get_id() != m_impl->ioThread.get_id()) {
      m_impl->ioThread.join();
    } else {
      m_impl->ioThread.detach();
    }
  }
}

void NativeWebSocketClient::sendText(const std::string &message) {
  if (!m_connected || !m_impl->running)
    return;

  try {
    if (m_impl->useSSL && m_impl->wss && m_impl->wss->is_open()) {
      m_impl->wss->write(net::buffer(message));
    } else if (!m_impl->useSSL && m_impl->ws && m_impl->ws->is_open()) {
      m_impl->ws->write(net::buffer(message));
    }
  } catch (std::exception const &e) {
    // Silently ignore errors if we're disconnecting
    if (m_connected && m_impl->running) {
      std::cerr << "❌ Send error: " << e.what() << std::endl;
    }
  }
}

void NativeWebSocketClient::sendBinary(const std::vector<uint8_t> &data) {
  if (!m_connected)
    return;

  try {
    if (m_impl->useSSL && m_impl->wss) {
      m_impl->wss->binary(true);
      m_impl->wss->write(net::buffer(data));
      m_impl->wss->binary(false);
    } else if (!m_impl->useSSL && m_impl->ws) {
      m_impl->ws->binary(true);
      m_impl->ws->write(net::buffer(data));
      m_impl->ws->binary(false);
    }
  } catch (std::exception const &e) {
    std::cerr << "❌ Send error: " << e.what() << std::endl;
  }
}

void NativeWebSocketClient::sendEngineIOPing() {
  sendText("2");
  std::lock_guard<std::mutex> lock(m_statsMutex);
  m_lastPing = std::chrono::system_clock::now();
}

void NativeWebSocketClient::sendEngineIOPong() { sendText("3"); }

void NativeWebSocketClient::sendSocketIOConnect() {
  // Send "40/" to connect to root namespace (like Go implementation)
  sendText("40/");
  std::cout << "📤 Sent Socket.IO connect: 40/" << std::endl;
}

void NativeWebSocketClient::onMessage(const std::string &message) {
  m_messagesReceived++;

  // Parse Engine.IO/Socket.IO protocol (from Go reference)
  // Note: parseEngineIOPacket will call m_onMessage for Socket.IO events only
  parseEngineIOPacket(message);
}

void NativeWebSocketClient::parseEngineIOPacket(const std::string &packet) {
  if (packet.empty())
    return;

  char packetType = packet[0];

  switch (packetType) {
  case '0': { // Engine.IO open (handshake)
    std::cout << "📨 Engine.IO handshake: " << packet.substr(1) << std::endl;

    // Parse handshake JSON to get server's pingInterval
    // Example:
    // {"sid":"...","upgrades":[],"pingInterval":20000,"pingTimeout":60000}
    try {
      std::string json = packet.substr(1);
      size_t pingIntervalPos = json.find("\"pingInterval\":");
      if (pingIntervalPos != std::string::npos) {
        size_t numStart = json.find(':', pingIntervalPos) + 1;
        size_t numEnd = json.find_first_of(",}", numStart);
        if (numEnd != std::string::npos) {
          std::string intervalStr = json.substr(numStart, numEnd - numStart);
          int interval = std::stoi(intervalStr);
          m_pingIntervalMs.store(interval);
          std::cout << "📊 Server pingInterval: " << interval << "ms"
                    << std::endl;
        }
      }
    } catch (...) {
      // Keep default 20000ms if parsing fails
    }

    // Note: Don't send immediate ping - wait for heartbeat thread to send first
    // ping at 25s interval This matches Go implementation behavior
    break;
  }

  case '2': // Engine.IO ping from server
    std::cout << "⬅️ Received ping, sending pong..." << std::endl;
    sendEngineIOPong();
    break;

  case '3': { // Engine.IO pong from server
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_lastPong = std::chrono::system_clock::now();
    break;
  }

  case '4': // Engine.IO message (contains Socket.IO payload)
    if (packet.length() > 1) {
      parseSocketIOEvent(packet.substr(1));
    }
    break;

  default:
    std::cout << "📨 Unknown Engine.IO packet: " << packet << std::endl;
    break;
  }
}

void NativeWebSocketClient::parseSocketIOEvent(const std::string &payload) {
  if (payload.empty())
    return;

  // Socket.IO event format: "0" for connect, "1" for disconnect, "2[event_name,
  // data]" for event
  if (payload[0] == '0') {
    std::cout << "✅ Socket.IO namespace connected (server confirmed)"
              << std::endl;
    //  Do NOT send another 40/ here - that would create duplicate connection
    // The server has confirmed our initial 40/ - no acknowledgment needed
    return;
  }

  if (payload[0] == '1') {
    std::cout << "⚠️ Socket.IO disconnect event (41) received from server"
              << std::endl;
    // Server requested disconnect - stop running to exit read loop
    // Heartbeat will be stopped in cleanup code (avoid double-join)
    m_impl->running = false;
    m_connected = false;
    return;
  }

  if (payload[0] == '2' && payload.length() > 1) {
    // Socket.IO event - extract JSON array: "2[event_name, data]" ->
    // "[event_name, data]"
    std::string eventData = payload.substr(1);

    // Pass only the JSON array to the application callback
    if (m_onMessage && !eventData.empty()) {
      m_onMessage(eventData);
    }
  }
}

void NativeWebSocketClient::startHeartbeat() {
  // Engine.IO Protocol: Client MUST send periodic pings to keep connection
  // alive Verified in working Go implementation: sends "2" (ping) every 25
  // seconds Server expects this to prove client is still active

  m_heartbeatRunning = true;
  m_heartbeatThread = std::thread([this]() {
    std::cout
        << "💓 Heartbeat started (25s interval, matching Go implementation)"
        << std::endl;

    while (m_heartbeatRunning) {
      std::this_thread::sleep_for(std::chrono::seconds(25));

      if (!m_heartbeatRunning)
        break;

      sendEngineIOPing();
    }

    std::cout << "💓 Heartbeat stopped" << std::endl;
  });
}

void NativeWebSocketClient::stopHeartbeat() {
  m_heartbeatRunning = false;
  if (m_heartbeatThread.joinable()) {
    m_heartbeatThread.join();
  }
}

void NativeWebSocketClient::attemptReconnect() {
  m_reconnectAttempts++;

  // Exponential backoff (1s, 2s, 4s, 8s, ..., max 60s)
  int delaySec = std::min(1 << (m_reconnectAttempts - 1), 60);

  std::cout << "🔄 Reconnecting in " << delaySec << " seconds (attempt "
            << m_reconnectAttempts << "/10)" << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(delaySec));

  // Reconnect with same callbacks
  connect(m_impl->currentUrl, m_onConnected, m_onDisconnected, m_onError);
}

void NativeWebSocketClient::parseUrl(const std::string &url) {
  // Parse wss://host:port/path?query or ws://host:port/path?query
  std::string cleanUrl = url;

  // Detect SSL from protocol
  m_impl->useSSL = (url.find("wss://") == 0);

  // Remove protocol
  size_t protoEnd = cleanUrl.find("://");
  if (protoEnd != std::string::npos) {
    cleanUrl = cleanUrl.substr(protoEnd + 3);
  }

  // Extract host and rest
  size_t pathStart = cleanUrl.find('/');
  std::string hostPort = cleanUrl.substr(0, pathStart);
  m_impl->path =
      (pathStart != std::string::npos) ? cleanUrl.substr(pathStart) : "/";

  // Split host and port
  size_t portDelim = hostPort.find(':');
  if (portDelim != std::string::npos) {
    m_impl->host = hostPort.substr(0, portDelim);
    m_impl->port = hostPort.substr(portDelim + 1);
  } else {
    m_impl->host = hostPort;
    m_impl->port = m_impl->useSSL ? "443" : "80"; // Default ports
  }
}

NativeWebSocketClient::ConnectionStats NativeWebSocketClient::getStats() const {
  std::lock_guard<std::mutex> lock(m_statsMutex);
  return {
      .connected = m_connected.load(),
      .reconnectCount = m_reconnectAttempts.load(),
      .messagesReceived = m_messagesReceived.load(),
      .lastPing = m_lastPing,
      .lastPong = m_lastPong,
      .subscriptions = 0 // TODO: track subscriptions
  };
}

std::string NativeWebSocketClient::getHealthStatus() const {
  if (!m_connected)
    return "DISCONNECTED";

  std::lock_guard<std::mutex> lock(m_statsMutex);
  auto now = std::chrono::system_clock::now();
  auto timeSinceLastPong =
      std::chrono::duration_cast<std::chrono::seconds>(now - m_lastPong)
          .count();

  if (timeSinceLastPong < 40) {
    return "HEALTHY";
  } else if (timeSinceLastPong < 60) {
    return "DEGRADED";
  }
  return "UNHEALTHY";
}

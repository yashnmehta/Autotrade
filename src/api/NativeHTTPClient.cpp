#include "api/NativeHTTPClient.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <iostream>
#include <regex>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

struct NativeHTTPClient::Impl {
    net::io_context ioc;
};

NativeHTTPClient::NativeHTTPClient()
    : m_impl(std::make_unique<Impl>())
    , m_timeout(30)
{
}

NativeHTTPClient::~NativeHTTPClient() = default;

void NativeHTTPClient::setTimeout(int seconds)
{
    m_timeout = seconds;
}

NativeHTTPClient::Response NativeHTTPClient::get(const std::string& url,
                                                   const std::map<std::string, std::string>& headers)
{
    return makeRequest("GET", url, "", headers);
}

NativeHTTPClient::Response NativeHTTPClient::post(const std::string& url,
                                                    const std::string& body,
                                                    const std::map<std::string, std::string>& headers)
{
    return makeRequest("POST", url, body, headers);
}

NativeHTTPClient::Response NativeHTTPClient::put(const std::string& url,
                                                   const std::string& body,
                                                   const std::map<std::string, std::string>& headers)
{
    return makeRequest("PUT", url, body, headers);
}

NativeHTTPClient::Response NativeHTTPClient::del(const std::string& url,
                                                   const std::map<std::string, std::string>& headers)
{
    return makeRequest("DELETE", url, "", headers);
}

NativeHTTPClient::Response NativeHTTPClient::makeRequest(const std::string& method,
                                                           const std::string& url,
                                                           const std::string& body,
                                                           const std::map<std::string, std::string>& headers)
{
    Response response;
    response.success = false;
    
    try {
        // Parse URL: protocol://host:port/path
        std::regex urlRegex(R"(^(https?)://([^:/]+)(?::(\d+))?(/.*)?$)");
        std::smatch match;
        
        if (!std::regex_match(url, match, urlRegex)) {
            response.error = "Invalid URL format";
            return response;
        }
        
        std::string protocol = match[1].str();
        std::string host = match[2].str();
        std::string port = match[3].str();
        std::string target = match[4].str();
        
        if (target.empty()) target = "/";
        if (port.empty()) {
            port = (protocol == "https") ? "443" : "80";
        }
        
        bool useSSL = (protocol == "https");
        
        // Resolve host
        tcp::resolver resolver(m_impl->ioc);
        auto const results = resolver.resolve(host, port);
        
        if (useSSL) {
            // HTTPS request
            ssl::context ctx(ssl::context::tlsv12_client);
            ctx.set_default_verify_paths();
            ctx.set_verify_mode(ssl::verify_none); // Skip certificate verification (like our WebSocket)
            
            beast::ssl_stream<beast::tcp_stream> stream(m_impl->ioc, ctx);
            
            // SNI
            if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
                throw beast::system_error(
                    beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()),
                    "Failed to set SNI hostname"
                );
            }
            
            // Connect
            beast::get_lowest_layer(stream).connect(results);
            stream.handshake(ssl::stream_base::client);
            
            // Build request
            http::request<http::string_body> req;
            if (method == "GET") req.method(http::verb::get);
            else if (method == "POST") req.method(http::verb::post);
            else if (method == "PUT") req.method(http::verb::put);
            else if (method == "DELETE") req.method(http::verb::delete_);
            
            req.target(target);
            req.version(11);
            req.set(http::field::host, host);
            req.set(http::field::user_agent, "TradingTerminal/1.0");
            
            // Add custom headers
            for (const auto& [key, value] : headers) {
                req.set(key, value);
            }
            
            // Add body if present
            if (!body.empty()) {
                req.body() = body;
                req.prepare_payload();
            }
            
            // Send request
            http::write(stream, req);
            
            // Receive response (with 50MB limit for large CSV files)
            beast::flat_buffer buffer;
            http::response_parser<http::string_body> parser;
            parser.body_limit(50 * 1024 * 1024); // 50MB limit for master contracts CSV
            http::read(stream, buffer, parser);
            http::response<http::string_body> res = parser.release();
            
            // Fill response
            response.statusCode = res.result_int();
            response.body = res.body();
            for (auto const& field : res) {
                response.headers[std::string(field.name_string())] = std::string(field.value());
            }
            response.success = (response.statusCode >= 200 && response.statusCode < 300);
            
            // Shutdown SSL
            beast::error_code ec;
            stream.shutdown(ec);
            // Ignore "stream truncated" error on shutdown
            
        } else {
            // HTTP request (plain)
            beast::tcp_stream stream(m_impl->ioc);
            stream.connect(results);
            
            // Build request
            http::request<http::string_body> req;
            if (method == "GET") req.method(http::verb::get);
            else if (method == "POST") req.method(http::verb::post);
            else if (method == "PUT") req.method(http::verb::put);
            else if (method == "DELETE") req.method(http::verb::delete_);
            
            req.target(target);
            req.version(11);
            req.set(http::field::host, host);
            req.set(http::field::user_agent, "TradingTerminal/1.0");
            
            // Add custom headers
            for (const auto& [key, value] : headers) {
                req.set(key, value);
            }
            
            // Add body if present
            if (!body.empty()) {
                req.body() = body;
                req.prepare_payload();
            }
            
            // Send request
            http::write(stream, req);
            
            // Receive response (with 50MB limit for large CSV files)
            beast::flat_buffer buffer;
            http::response_parser<http::string_body> parser;
            parser.body_limit(50 * 1024 * 1024); // 50MB limit for master contracts CSV
            http::read(stream, buffer, parser);
            http::response<http::string_body> res = parser.release();
            
            // Fill response
            response.statusCode = res.result_int();
            response.body = res.body();
            for (auto const& field : res) {
                response.headers[std::string(field.name_string())] = std::string(field.value());
            }
            response.success = (response.statusCode >= 200 && response.statusCode < 300);
            
            // Shutdown
            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        }
        
    } catch (std::exception const& e) {
        response.error = e.what();
        response.success = false;
    }
    
    return response;
}

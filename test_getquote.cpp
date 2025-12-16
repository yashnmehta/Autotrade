/**
 * Standalone test for XTS getQuote API
 * 
 * Tests different parameters:
 * - Different exchange segments (NSECM=1, NSEFO=2, NSECD=13, BSECM=11, BSEFO=12, BSECD=61, MCXFO=51)
 * - Different message codes (1501=touchline, 1502=market depth, 1510=candles, 1512=open interest)
 * - Different instruments (equity, futures, options)
 * 
 * Compile: g++ -std=c++11 test_getquote.cpp -o test_getquote -lcurl
 * Run: ./test_getquote <token> <base_url>
 */

#include <iostream>
#include <string>
#include <curl/curl.h>
#include <sstream>

// Callback to capture HTTP response
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

struct TestCase {
    std::string name;
    int exchangeSegment;
    long long exchangeInstrumentID;
    int xtsMessageCode;
    std::string description;
};

void testGetQuote(const std::string& baseURL, const std::string& token, const TestCase& test) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL" << std::endl;
        return;
    }
    
    std::string url = baseURL + "/instruments/quotes";
    std::string response;
    
    // Build JSON payload
    std::ostringstream jsonPayload;
    jsonPayload << "{"
                << "\"instruments\":[{"
                << "\"exchangeSegment\":" << test.exchangeSegment << ","
                << "\"exchangeInstrumentID\":" << test.exchangeInstrumentID
                << "}],"
                << "\"xtsMessageCode\":" << test.xtsMessageCode
                << "}";
    
    std::string payload = jsonPayload.str();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "TEST: " << test.name << std::endl;
    std::cout << "Description: " << test.description << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "URL: " << url << std::endl;
    std::cout << "Request Body: " << payload << std::endl;
    std::cout << "Authorization: " << token.substr(0, 20) << "..." << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // Setup CURL
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string authHeader = "Authorization: " + token;
    headers = curl_slist_append(headers, authHeader.c_str());
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);  // Verbose mode to see full request/response
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "CURL Error: " << curl_easy_strerror(res) << std::endl;
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        std::cout << "HTTP Status: " << http_code << std::endl;
        std::cout << "Response Body:" << std::endl;
        std::cout << response << std::endl;
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    std::cout << "========================================\n" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <auth_token> <base_url>" << std::endl;
        std::cerr << "Example: " << argv[0] << " \"eyJhbGc...\" \"http://192.168.102.9:3000/apimarketdata\"" << std::endl;
        return 1;
    }
    
    std::string token = argv[1];
    std::string baseURL = argv[2];
    
    std::cout << "XTS getQuote API Test Suite" << std::endl;
    std::cout << "============================" << std::endl;
    
    // Test cases covering different scenarios
    std::vector<TestCase> tests = {
        // NSE F&O (Futures & Options)
        {"NIFTY Future - Message Code 1502", 2, 49543, 1502, "NIFTY future with full market depth"},
        {"NIFTY Future - Message Code 1501", 2, 49543, 1501, "NIFTY future with touchline only"},
        {"BANKNIFTY Future - Message Code 1502", 2, 59175, 1502, "BANKNIFTY future with full market depth"},
        
        // NSE Options
        {"NIFTY CE Option - Message Code 1502", 2, 50000, 1502, "NIFTY call option (if valid token)"},
        
        // NSE Cash Market (if you have tokens)
        {"NSE Equity - Message Code 1502", 1, 2885, 1502, "NSE equity (RELIANCE example token)"},
        {"NSE Equity - Message Code 1501", 1, 2885, 1501, "NSE equity with touchline only"},
        
        // BSE (if you have BSE tokens)
        {"BSE Equity - Message Code 1502", 11, 500325, 1502, "BSE equity example"},
        
        // Test with different message codes
        {"NIFTY - Message Code 1510", 2, 49543, 1510, "NIFTY with candle data (if supported)"},
        {"NIFTY - Message Code 1512", 2, 49543, 1512, "NIFTY with OI data (if supported)"},
    };
    
    // Run all tests
    for (const auto& test : tests) {
        testGetQuote(baseURL, token, test);
        // Small delay between requests
        sleep(1);
    }
    
    std::cout << "\nAll tests completed!" << std::endl;
    return 0;
}

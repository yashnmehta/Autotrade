/**
 * Standalone test for XTS subscribe/unsubscribe API
 * 
 * Tests different scenarios:
 * - First-time subscription (should return initial snapshot with touchline)
 * - Re-subscription (already subscribed token - should return success but no snapshot)
 * - Multiple instruments in single request
 * - Unsubscription
 * - Different exchange segments
 * 
 * Compile: g++ -std=c++11 test_subscription.cpp -o test_subscription -lcurl
 * Run: ./test_subscription <token> <base_url>
 */

#include <iostream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <sstream>
#include <unistd.h>

// Callback to capture HTTP response
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

struct Instrument {
    int exchangeSegment;
    long long exchangeInstrumentID;
    std::string name;
};

std::string buildSubscriptionPayload(const std::vector<Instrument>& instruments, int messageCode) {
    std::ostringstream json;
    json << "{\"instruments\":[";
    
    for (size_t i = 0; i < instruments.size(); i++) {
        if (i > 0) json << ",";
        json << "{"
             << "\"exchangeSegment\":" << instruments[i].exchangeSegment << ","
             << "\"exchangeInstrumentID\":" << instruments[i].exchangeInstrumentID
             << "}";
    }
    
    json << "],\"xtsMessageCode\":" << messageCode << "}";
    return json.str();
}

std::string performRequest(const std::string& url, const std::string& token, 
                          const std::string& payload, bool verbose = false) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return "ERROR: Failed to initialize CURL";
    }
    
    std::string response;
    
    // Setup headers
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string authHeader = "Authorization: " + token;
    headers = curl_slist_append(headers, authHeader.c_str());
    
    // Setup CURL options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    if (verbose) {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        response = "CURL Error: " + std::string(curl_easy_strerror(res));
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        response = "HTTP " + std::to_string(http_code) + "\n" + response;
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return response;
}

void testSubscription(const std::string& baseURL, const std::string& token, 
                     const std::string& testName, const std::vector<Instrument>& instruments,
                     int messageCode = 1502) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "TEST: " << testName << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    std::string url = baseURL + "/instruments/subscription";
    std::string payload = buildSubscriptionPayload(instruments, messageCode);
    
    std::cout << "URL: " << url << std::endl;
    std::cout << "Instruments: ";
    for (const auto& inst : instruments) {
        std::cout << inst.name << "(" << inst.exchangeInstrumentID << ") ";
    }
    std::cout << std::endl;
    std::cout << "Request Body: " << payload << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    std::string response = performRequest(url, token, payload, false);
    std::cout << "Response:" << std::endl << response << std::endl;
    std::cout << "========================================\n" << std::endl;
}

void testUnsubscription(const std::string& baseURL, const std::string& token,
                       const std::string& testName, const std::vector<Instrument>& instruments) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "TEST: " << testName << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    std::string url = baseURL + "/instruments/subscription";
    std::string payload = buildSubscriptionPayload(instruments, 1502);
    
    std::cout << "URL: " << url << std::endl;
    std::cout << "Method: DELETE (unsubscribe)" << std::endl;
    std::cout << "Instruments: ";
    for (const auto& inst : instruments) {
        std::cout << inst.name << "(" << inst.exchangeInstrumentID << ") ";
    }
    std::cout << std::endl;
    std::cout << "Request Body: " << payload << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // For unsubscribe, we typically use DELETE method or specific endpoint
    // Check XTS API documentation for exact endpoint
    CURL* curl = curl_easy_init();
    if (curl) {
        std::string response;
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string authHeader = "Authorization: " + token;
        headers = curl_slist_append(headers, authHeader.c_str());
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        CURLcode res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            std::cout << "HTTP Status: " << http_code << std::endl;
            std::cout << "Response: " << response << std::endl;
        } else {
            std::cout << "Error: " << curl_easy_strerror(res) << std::endl;
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    
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
    
    std::cout << "XTS Subscription API Test Suite" << std::endl;
    std::cout << "================================" << std::endl;
    
    // Define test instruments (adjust tokens based on your actual data)
    Instrument nifty = {2, 49543, "NIFTY"};
    Instrument banknifty = {2, 59175, "BANKNIFTY"};
    Instrument reliance = {1, 2885, "RELIANCE"};  // Example NSE equity token
    
    // Test 1: Subscribe single instrument (first time)
    std::cout << "\n>>> SCENARIO 1: First-time subscription (should return snapshot)" << std::endl;
    testSubscription(baseURL, token, "Subscribe NIFTY (First Time)", {nifty});
    sleep(2);
    
    // Test 2: Re-subscribe same instrument (already subscribed)
    std::cout << "\n>>> SCENARIO 2: Re-subscribe already subscribed instrument" << std::endl;
    testSubscription(baseURL, token, "Re-subscribe NIFTY (Already Subscribed)", {nifty});
    sleep(2);
    
    // Test 3: Subscribe multiple instruments at once
    std::cout << "\n>>> SCENARIO 3: Subscribe multiple instruments in one request" << std::endl;
    testSubscription(baseURL, token, "Subscribe NIFTY + BANKNIFTY", {nifty, banknifty});
    sleep(2);
    
    // Test 4: Subscribe different exchange segment
    std::cout << "\n>>> SCENARIO 4: Subscribe NSE Cash Market instrument" << std::endl;
    testSubscription(baseURL, token, "Subscribe RELIANCE (NSE CM)", {reliance});
    sleep(2);
    
    // Test 5: Subscribe with different message code (touchline only)
    std::cout << "\n>>> SCENARIO 5: Subscribe with message code 1501 (touchline)" << std::endl;
    testSubscription(baseURL, token, "Subscribe NIFTY with Touchline Only", {nifty}, 1501);
    sleep(2);
    
    // Test 6: Unsubscribe
    std::cout << "\n>>> SCENARIO 6: Unsubscribe instrument" << std::endl;
    testUnsubscription(baseURL, token, "Unsubscribe BANKNIFTY", {banknifty});
    sleep(2);
    
    // Test 7: Re-subscribe after unsubscribe (should return snapshot again)
    std::cout << "\n>>> SCENARIO 7: Re-subscribe after unsubscribe" << std::endl;
    testSubscription(baseURL, token, "Re-subscribe BANKNIFTY after Unsubscribe", {banknifty});
    
    std::cout << "\n\nAll tests completed!" << std::endl;
    std::cout << "\nKEY OBSERVATIONS TO NOTE:" << std::endl;
    std::cout << "1. Does first subscription return 'listQuotes' with touchline data?" << std::endl;
    std::cout << "2. Does re-subscription (already subscribed) return success but no listQuotes?" << std::endl;
    std::cout << "3. What's the structure of the touchline data in listQuotes?" << std::endl;
    std::cout << "4. Does unsubscribe use DELETE method or different endpoint?" << std::endl;
    std::cout << "5. After unsubscribe, does re-subscribe return snapshot again?" << std::endl;
    
    return 0;
}

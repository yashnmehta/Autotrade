/**
 * Standalone Test for XTS Interactive Trading Data
 * Tests Positions, Orders, and Trades endpoints using NativeHTTPClient
 */

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include "api/NativeHTTPClient.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QSettings>
#include <QFile>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "========================================" << std::endl;
    std::cout << "XTS INTERACTIVE DATA TEST (NATIVE)" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // Load credentials from config.ini
    QString configPath = "/Users/yashmehta/Desktop/go_proj/trading_terminal_cpp/configs/config.ini";
    if (!QFile::exists(configPath)) {
        std::cerr << "Config file not found: " << configPath.toStdString() << std::endl;
        return 1;
    }

    QSettings settings(configPath, QSettings::IniFormat);
    QString baseURL = settings.value("XTS/url").toString();
    QString appKey = settings.value("CREDENTIALS/interactive_appkey").toString();
    QString secretKey = settings.value("CREDENTIALS/interactive_secretkey").toString();
    QString source = settings.value("CREDENTIALS/source", "TWSAPI").toString();

    if (baseURL.isEmpty() || appKey.isEmpty() || secretKey.isEmpty()) {
        std::cerr << "Missing credentials in config.ini" << std::endl;
        return 1;
    }

    std::cout << "Base URL:   " << baseURL.toStdString() << std::endl;
    std::cout << "App Key:    " << appKey.toStdString() << std::endl;
    std::cout << "Source:     " << source.toStdString() << std::endl;

    NativeHTTPClient client;
    
    // 1. Login
    std::cout << "\n[1] Logging in..." << std::endl;
    QJsonObject loginData;
    loginData["appKey"] = appKey;
    loginData["secretKey"] = secretKey;
    loginData["source"] = source;

    std::string loginUrl = (baseURL + "/interactive/user/session").toStdString();
    auto loginResp = client.post(loginUrl, QJsonDocument(loginData).toJson().toStdString(), {{"Content-Type", "application/json"}});

    if (!loginResp.success) {
        std::cerr << "Login failed! Status: " << loginResp.statusCode << " Error: " << loginResp.error << std::endl;
        std::cerr << "Body: " << loginResp.body << std::endl;
        return 1;
    }

    QJsonDocument loginDoc = QJsonDocument::fromJson(QByteArray::fromStdString(loginResp.body));
    QJsonObject resultObj = loginDoc.object()["result"].toObject();
    QString token = resultObj["token"].toString();
    QJsonArray clientCodes = resultObj["clientCodes"].toArray();
    
    if (token.isEmpty()) {
        std::cerr << "Failed to get token from login response" << std::endl;
        return 1;
    }
    std::cout << "✓ Login successful. Token obtained." << std::endl;
    std::cout << "Available Client Codes: " << clientCodes.size() << std::endl;
    for (int i = 0; i < clientCodes.size(); ++i) {
        std::cout << "  [" << i << "] " << clientCodes[i].toString().toStdString() << std::endl;
    }

    std::map<std::string, std::string> headers;
    headers["Authorization"] = token.toStdString();
    headers["Content-Type"] = "application/json";

    // Test for each client code
    for (int i = 0; i < clientCodes.size(); ++i) {
        QString clientID = clientCodes[i].toString();
        std::cout << "\n----------------------------------------" << std::endl;
        std::cout << "TESTING FOR CLIENT: " << clientID.toStdString() << std::endl;
        std::cout << "----------------------------------------" << std::endl;

        // 2. Get Positions (NetWise)
        std::cout << "[2] Fetching Positions (NetWise) for " << clientID.toStdString() << "..." << std::endl;
        std::string posUrl = (baseURL + "/interactive/portfolio/positions?dayOrNet=NetWise&clientID=" + clientID).toStdString();
        auto posResp = client.get(posUrl, headers);
        
        QJsonDocument posDoc = QJsonDocument::fromJson(QByteArray::fromStdString(posResp.body));
        if (posDoc.object()["type"].toString() == "success") {
            QJsonArray arr = posDoc.object()["result"].toObject()["positionList"].toArray();
            std::cout << "  ✓ Success! Positions: " << arr.size() << std::endl;
            
            QFile file("dump_positions.json");
            if (file.open(QIODevice::WriteOnly)) {
                file.write(posDoc.toJson());
                file.close();
                std::cout << "  -> Dumped to dump_positions.json" << std::endl;
            }
        } else {
            std::cout << "  ✗ " << posDoc.object()["description"].toString().toStdString() << std::endl;
        }

        // 3. Get Orders
        std::cout << "[3] Fetching Orders for " << clientID.toStdString() << "..." << std::endl;
        std::string ordUrl = (baseURL + "/interactive/orders?clientID=" + clientID).toStdString();
        auto ordResp = client.get(ordUrl, headers);
        QJsonDocument ordDoc = QJsonDocument::fromJson(QByteArray::fromStdString(ordResp.body));
        if (ordDoc.object()["type"].toString() == "success") {
            QJsonArray arr = ordDoc.object()["result"].toArray();
            std::cout << "  ✓ Success! Orders: " << arr.size() << std::endl;
            
            QFile file("dump_orders.json");
            if (file.open(QIODevice::WriteOnly)) {
                file.write(ordDoc.toJson());
                file.close();
                std::cout << "  -> Dumped to dump_orders.json" << std::endl;
            }
        } else {
            std::cout << "  ✗ " << ordDoc.object()["description"].toString().toStdString() << std::endl;
        }

        // 4. Get Trades
        std::cout << "[4] Fetching Trades for " << clientID.toStdString() << "..." << std::endl;
        std::string trdUrl = (baseURL + "/interactive/orders/trades?clientID=" + clientID).toStdString();
        auto trdResp = client.get(trdUrl, headers);
        QJsonDocument trdDoc = QJsonDocument::fromJson(QByteArray::fromStdString(trdResp.body));
        if (trdDoc.object()["type"].toString() == "success") {
            QJsonArray arr = trdDoc.object()["result"].toArray();
            std::cout << "  ✓ Success! Trades: " << arr.size() << std::endl;
            
            QFile file("dump_trades.json");
            if (file.open(QIODevice::WriteOnly)) {
                file.write(trdDoc.toJson());
                file.close();
                std::cout << "  -> Dumped to dump_trades.json" << std::endl;
            }
        } else {
            std::cout << "  ✗ " << trdDoc.object()["description"].toString().toStdString() << std::endl;
        }
    }

    std::cout << "\nTest Finished." << std::endl;
    return 0;
}

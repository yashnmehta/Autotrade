/**
 * XTS Market Data API Test Suite
 * Tests all critical Market Data endpoints with actual API calls
 */

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
#include <QDebug>
#include <iostream>
#include <iomanip>

class XTSMarketDataTester : public QObject {
    Q_OBJECT

public:
    XTSMarketDataTester(QObject *parent = nullptr) 
        : QObject(parent)
        , m_networkManager(new QNetworkAccessManager(this))
    {
        // XTS Market Data API Configuration
        m_baseURL = "https://mtrade.arhamshare.com/apimarketdata";
        m_appKey = "2d832e8d71e1d180aee499";
        m_secretKey = "Snvd485$cC";
        m_source = "WEBAPI";
        
        m_testsPassed = 0;
        m_testsFailed = 0;
        m_testsTotal = 0;
    }

    void runAllTests() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "XTS MARKET DATA API - TEST SUITE" << std::endl;
        std::cout << "========================================\n" << std::endl;
        
        // Start with login
        testLogin();
    }

private slots:
    void testLogin() {
        std::cout << "TEST 1: Login to Market Data API" << std::endl;
        std::cout << "Endpoint: POST /auth/login" << std::endl;
        
        QJsonObject loginData;
        loginData["appKey"] = m_appKey;
        loginData["secretKey"] = m_secretKey;
        loginData["source"] = m_source;
        
        QUrl requestUrl(m_baseURL + "/auth/login"); QNetworkRequest request(requestUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        
        QJsonDocument doc(loginData);
        QNetworkReply *reply = m_networkManager->post(request, doc.toJson());
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            
            if (reply->error() == QNetworkReply::NoError) {
                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                QJsonObject obj = doc.object();
                
                if (obj["type"].toString() == "success") {
                    m_authToken = obj["result"].toObject()["token"].toString();
                    std::cout << "✓ Login successful" << std::endl;
                    std::cout << "  Token: " << m_authToken.left(30).toStdString() << "..." << std::endl;
                    m_testsPassed++;
                    
                    // Continue with next test
                    QTimer::singleShot(500, this, &XTSMarketDataTester::testConfig);
                } else {
                    std::cout << "✗ Login failed: " << obj["description"].toString().toStdString() << std::endl;
                    m_testsFailed++;
                    finishTests();
                }
            } else {
                std::cout << "✗ Network error: " << reply->errorString().toStdString() << std::endl;
                std::cout << "  Response: " << reply->readAll().toStdString() << std::endl;
                m_testsFailed++;
                finishTests();
            }
            
            reply->deleteLater();
        });
    }

    void testConfig() {
        std::cout << "\nTEST 2: Get Client Config" << std::endl;
        std::cout << "Endpoint: GET /config/clientConfig" << std::endl;
        
        QUrl requestUrl(m_baseURL + "/config/clientConfig"); QNetworkRequest request(requestUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", m_authToken.toUtf8());
        
        QNetworkReply *reply = m_networkManager->get(request);
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            handleTestResponse(reply, "Client Config");
            QTimer::singleShot(500, this, &XTSMarketDataTester::testIndexList);
            reply->deleteLater();
        });
    }

    void testIndexList() {
        std::cout << "\nTEST 3: Get Index List" << std::endl;
        std::cout << "Endpoint: GET /instruments/indexlist?exchangeSegment=1" << std::endl;
        
        QUrl requestUrl(m_baseURL + "/instruments/indexlist?exchangeSegment=1"); QNetworkRequest request(requestUrl);
        request.setRawHeader("Authorization", m_authToken.toUtf8());
        
        QNetworkReply *reply = m_networkManager->get(request);
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            handleTestResponse(reply, "Index List");
            QTimer::singleShot(500, this, &XTSMarketDataTester::testSearchInstruments);
            reply->deleteLater();
        });
    }

    void testSearchInstruments() {
        std::cout << "\nTEST 4: Search Instruments" << std::endl;
        std::cout << "Endpoint: GET /search/instruments?searchString=RELIANCE" << std::endl;
        
        QUrl requestUrl(m_baseURL + "/search/instruments?searchString=RELIANCE"); QNetworkRequest request(requestUrl);
        request.setRawHeader("Authorization", m_authToken.toUtf8());
        
        QNetworkReply *reply = m_networkManager->get(request);
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            
            if (reply->error() == QNetworkReply::NoError) {
                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                QJsonObject obj = doc.object();
                
                if (obj["type"].toString() == "success") {
                    QJsonArray results = obj["result"].toArray();
                    std::cout << "✓ Search successful - Found " << results.size() << " instruments" << std::endl;
                    
                    if (results.size() > 0) {
                        QJsonObject first = results[0].toObject();
                        std::cout << "  First result: " << first["Name"].toString().toStdString() << std::endl;
                        std::cout << "  Exchange Segment: " << first["ExchangeSegment"].toInt() << std::endl;
                        std::cout << "  Instrument ID: " << first["ExchangeInstrumentID"].toVariant().toLongLong() << std::endl;
                    }
                    m_testsPassed++;
                } else {
                    std::cout << "✗ Search failed: " << obj["description"].toString().toStdString() << std::endl;
                    m_testsFailed++;
                }
            } else {
                std::cout << "✗ Network error: " << reply->errorString().toStdString() << std::endl;
                m_testsFailed++;
            }
            
            QTimer::singleShot(500, this, &XTSMarketDataTester::testQuote);
            reply->deleteLater();
        });
    }

    void testQuote() {
        std::cout << "\nTEST 5: Get Quote" << std::endl;
        std::cout << "Endpoint: POST /instruments/quotes" << std::endl;
        
        QJsonObject requestData;
        QJsonArray instruments;
        QJsonObject inst;
        inst["exchangeSegment"] = 1;
        inst["exchangeInstrumentID"] = 2885; // RELIANCE
        instruments.append(inst);
        requestData["instruments"] = instruments;
        requestData["xtsMessageCode"] = 1504;
        requestData["publishFormat"] = "JSON";
        
        QUrl requestUrl(m_baseURL + "/instruments/quotes"); QNetworkRequest request(requestUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", m_authToken.toUtf8());
        
        QJsonDocument doc(requestData);
        QNetworkReply *reply = m_networkManager->post(request, doc.toJson());
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            handleTestResponse(reply, "Quote");
            QTimer::singleShot(500, this, &XTSMarketDataTester::testSubscribe);
            reply->deleteLater();
        });
    }

    void testSubscribe() {
        std::cout << "\nTEST 6: Subscribe to Quote" << std::endl;
        std::cout << "Endpoint: POST /instruments/subscription" << std::endl;
        
        QJsonObject requestData;
        QJsonArray instruments;
        QJsonObject inst;
        inst["exchangeSegment"] = 1;
        inst["exchangeInstrumentID"] = 2885;
        instruments.append(inst);
        requestData["instruments"] = instruments;
        requestData["xtsMessageCode"] = 1502;
        
        QUrl requestUrl(m_baseURL + "/instruments/subscription"); QNetworkRequest request(requestUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", m_authToken.toUtf8());
        
        QJsonDocument doc(requestData);
        QNetworkReply *reply = m_networkManager->post(request, doc.toJson());
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            handleTestResponse(reply, "Subscribe");
            QTimer::singleShot(500, this, &XTSMarketDataTester::testUnsubscribe);
            reply->deleteLater();
        });
    }

    void testUnsubscribe() {
        std::cout << "\nTEST 7: Unsubscribe from Quote" << std::endl;
        std::cout << "Endpoint: PUT /instruments/subscription" << std::endl;
        
        QJsonObject requestData;
        QJsonArray instruments;
        QJsonObject inst;
        inst["exchangeSegment"] = 1;
        inst["exchangeInstrumentID"] = 2885;
        instruments.append(inst);
        requestData["instruments"] = instruments;
        requestData["xtsMessageCode"] = 1502;
        
        QUrl requestUrl(m_baseURL + "/instruments/subscription"); QNetworkRequest request(requestUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", m_authToken.toUtf8());
        
        QJsonDocument doc(requestData);
        QNetworkReply *reply = m_networkManager->sendCustomRequest(request, "PUT", doc.toJson());
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            handleTestResponse(reply, "Unsubscribe");
            QTimer::singleShot(500, this, &XTSMarketDataTester::testMasterDownload);
            reply->deleteLater();
        });
    }

    void testMasterDownload() {
        std::cout << "\nTEST 8: Download Master Contracts" << std::endl;
        std::cout << "Endpoint: POST /instruments/master" << std::endl;
        
        QJsonObject requestData;
        QJsonArray segments;
        segments.append(1); // NSECM
        segments.append(2); // NSEFO
        requestData["exchangeSegmentList"] = segments;
        
        QUrl requestUrl(m_baseURL + "/instruments/master"); QNetworkRequest request(requestUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", m_authToken.toUtf8());
        
        QJsonDocument doc(requestData);
        QNetworkReply *reply = m_networkManager->post(request, doc.toJson());
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray data = reply->readAll();
                
                // Check if response is CSV data
                if (data.size() > 0 && data.contains("|")) {
                    std::cout << "✓ Master download successful" << std::endl;
                    std::cout << "  Downloaded " << data.size() << " bytes" << std::endl;
                    
                    // Count lines
                    int lines = data.count('\n');
                    std::cout << "  Contains " << lines << " instrument records" << std::endl;
                    
                    // Show first line as sample
                    int firstNewline = data.indexOf('\n');
                    if (firstNewline > 0) {
                        std::cout << "  Sample: " << data.left(firstNewline).toStdString() << std::endl;
                    }
                    
                    m_testsPassed++;
                } else {
                    std::cout << "✗ Master download returned unexpected format" << std::endl;
                    std::cout << "  Response: " << data.left(200).toStdString() << std::endl;
                    m_testsFailed++;
                }
            } else {
                std::cout << "✗ Network error: " << reply->errorString().toStdString() << std::endl;
                m_testsFailed++;
            }
            
            QTimer::singleShot(500, this, &XTSMarketDataTester::testLogout);
            reply->deleteLater();
        });
    }

    void testLogout() {
        std::cout << "\nTEST 9: Logout" << std::endl;
        std::cout << "Endpoint: DELETE /auth/logout" << std::endl;
        
        QUrl requestUrl(m_baseURL + "/auth/logout"); QNetworkRequest request(requestUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", m_authToken.toUtf8());
        
        QNetworkReply *reply = m_networkManager->sendCustomRequest(request, "DELETE");
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            handleTestResponse(reply, "Logout");
            finishTests();
            reply->deleteLater();
        });
    }

    void handleTestResponse(QNetworkReply *reply, const QString &testName) {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject obj = doc.object();
            
            if (obj["type"].toString() == "success") {
                std::cout << "✓ " << testName.toStdString() << " successful" << std::endl;
                m_testsPassed++;
            } else {
                std::cout << "✗ " << testName.toStdString() << " failed: " 
                          << obj["description"].toString().toStdString() << std::endl;
                m_testsFailed++;
            }
        } else {
            std::cout << "✗ " << testName.toStdString() << " network error: " 
                      << reply->errorString().toStdString() << std::endl;
            m_testsFailed++;
        }
    }

    void finishTests() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "TEST SUMMARY" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Total Tests:  " << m_testsTotal << std::endl;
        std::cout << "Passed:       " << m_testsPassed << " ✓" << std::endl;
        std::cout << "Failed:       " << m_testsFailed << " ✗" << std::endl;
        
        double successRate = m_testsTotal > 0 ? (m_testsPassed * 100.0 / m_testsTotal) : 0;
        std::cout << "Success Rate: " << std::fixed << std::setprecision(1) << successRate << "%" << std::endl;
        std::cout << "========================================\n" << std::endl;
        
        QCoreApplication::quit();
    }

private:
    QNetworkAccessManager *m_networkManager;
    QString m_baseURL;
    QString m_appKey;
    QString m_secretKey;
    QString m_source;
    QString m_authToken;
    
    int m_testsPassed;
    int m_testsFailed;
    int m_testsTotal;
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    XTSMarketDataTester tester;
    QTimer::singleShot(0, &tester, &XTSMarketDataTester::runAllTests);
    
    return app.exec();
}

#include "test_marketdata_api.moc"

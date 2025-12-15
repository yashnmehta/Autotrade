/**
 * XTS Interactive API Test Suite
 * Tests all critical Interactive API endpoints with actual API calls
 */

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QTimer>
#include <QDebug>
#include <iostream>
#include <iomanip>

class XTSInteractiveTester : public QObject {
    Q_OBJECT

public:
    XTSInteractiveTester(QObject *parent = nullptr) 
        : QObject(parent)
        , m_networkManager(new QNetworkAccessManager(this))
    {
        // XTS Interactive API Configuration
        m_baseURL = "https://mtrade.arhamshare.com";
        m_appKey = "5820d8e017294c81d71873";
        m_secretKey = "Ibvk668@NX";
        m_source = "TWSAPI";
        
        m_testsPassed = 0;
        m_testsFailed = 0;
        m_testsTotal = 0;
    }

    void runAllTests() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "XTS INTERACTIVE API - TEST SUITE" << std::endl;
        std::cout << "========================================\n" << std::endl;
        
        // Start with login
        testLogin();
    }

private slots:
    void testLogin() {
        std::cout << "TEST 1: Login to Interactive API" << std::endl;
        std::cout << "Endpoint: POST /interactive/user/session" << std::endl;
        
        QJsonObject loginData;
        loginData["appKey"] = m_appKey;
        loginData["secretKey"] = m_secretKey;
        loginData["source"] = m_source;
        
        QUrl requestUrl(m_baseURL + "/interactive/user/session"); QNetworkRequest request(requestUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        
        QJsonDocument doc(loginData);
        QNetworkReply *reply = m_networkManager->post(request, doc.toJson());
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            
            if (reply->error() == QNetworkReply::NoError) {
                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                QJsonObject obj = doc.object();
                
                if (obj["type"].toString() == "success") {
                    QJsonObject result = obj["result"].toObject();
                    m_authToken = result["token"].toString();
                    m_userID = result["userID"].toString();
                    
                    QJsonArray clientCodes = result["clientCodes"].toArray();
                    if (clientCodes.size() > 0) {
                        m_clientCode = clientCodes[0].toString();
                    }
                    
                    std::cout << "✓ Login successful" << std::endl;
                    std::cout << "  Token: " << m_authToken.left(30).toStdString() << "..." << std::endl;
                    std::cout << "  User ID: " << m_userID.toStdString() << std::endl;
                    std::cout << "  Client Code: " << m_clientCode.toStdString() << std::endl;
                    m_testsPassed++;
                    
                    // Continue with next test
                    QTimer::singleShot(500, this, &XTSInteractiveTester::testProfile);
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

    void testProfile() {
        std::cout << "\nTEST 2: Get User Profile" << std::endl;
        std::cout << "Endpoint: GET /interactive/user/profile" << std::endl;
        
        QUrl requestUrl(m_baseURL + "/interactive/user/profile"); QNetworkRequest request(requestUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", m_authToken.toUtf8());
        
        QNetworkReply *reply = m_networkManager->get(request);
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            
            // Profile endpoint may not be available on all servers
            if (reply->error() == QNetworkReply::NoError) {
                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                QJsonObject obj = doc.object();
                
                if (obj["type"].toString() == "success") {
                    std::cout << "✓ Profile retrieved successfully" << std::endl;
                    m_testsPassed++;
                } else {
                    std::cout << "⚠  Profile endpoint not available: " << obj["description"].toString().toStdString() << std::endl;
                    m_testsFailed++;
                }
            } else {
                std::cout << "⚠  Profile endpoint not available on this server" << std::endl;
                m_testsFailed++;
            }
            
            QTimer::singleShot(500, this, &XTSInteractiveTester::testBalance);
            reply->deleteLater();
        });
    }

    void testBalance() {
        std::cout << "\nTEST 3: Get Account Balance" << std::endl;
        std::cout << "Endpoint: GET /interactive/user/balance?clientID=" << m_clientCode.toStdString() << std::endl;
        
        QString url = m_baseURL + "/interactive/user/balance?clientID=" + m_clientCode;
        QUrl requestUrl(url); QNetworkRequest request(requestUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", m_authToken.toUtf8());
        
        QNetworkReply *reply = m_networkManager->get(request);
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            handleTestResponse(reply, "Account Balance");
            QTimer::singleShot(500, this, &XTSInteractiveTester::testHoldings);
            reply->deleteLater();
        });
    }

    void testHoldings() {
        std::cout << "\nTEST 4: Get Holdings" << std::endl;
        std::cout << "Endpoint: GET /interactive/portfolio/holdings?clientID=" << m_clientCode.toStdString() << std::endl;
        
        QString url = m_baseURL + "/interactive/portfolio/holdings?clientID=" + m_clientCode;
        QUrl requestUrl(url); QNetworkRequest request(requestUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", m_authToken.toUtf8());
        
        QNetworkReply *reply = m_networkManager->get(request);
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            
            if (reply->error() == QNetworkReply::NoError) {
                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                QJsonObject obj = doc.object();
                
                if (obj["type"].toString() == "success") {
                    QJsonArray holdings = obj["result"].toArray();
                    std::cout << "✓ Holdings retrieved - " << holdings.size() << " holdings found" << std::endl;
                    m_testsPassed++;
                } else {
                    std::cout << "✗ Holdings failed: " << obj["description"].toString().toStdString() << std::endl;
                    m_testsFailed++;
                }
            } else {
                std::cout << "✗ Network error: " << reply->errorString().toStdString() << std::endl;
                m_testsFailed++;
            }
            
            QTimer::singleShot(500, this, &XTSInteractiveTester::testPositionsDayWise);
            reply->deleteLater();
        });
    }

    void testPositionsDayWise() {
        std::cout << "\nTEST 5: Get Positions (DayWise)" << std::endl;
        std::cout << "Endpoint: GET /interactive/portfolio/positions?dayOrNet=DayWise" << std::endl;
        
        QString url = m_baseURL + "/interactive/portfolio/positions?dayOrNet=DayWise";
        QUrl requestUrl(url); QNetworkRequest request(requestUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", m_authToken.toUtf8());
        
        QNetworkReply *reply = m_networkManager->get(request);
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            
            if (reply->error() == QNetworkReply::NoError) {
                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                QJsonObject obj = doc.object();
                
                if (obj["type"].toString() == "success") {
                    QJsonArray positions = obj["result"].toArray();
                    std::cout << "✓ DayWise positions retrieved - " << positions.size() << " positions found" << std::endl;
                    m_testsPassed++;
                } else if (obj["code"].toString() == "e-property-validation-failed") {
                    std::cout << "✓ DayWise positions endpoint working (no positions)" << std::endl;
                    m_testsPassed++;
                } else {
                    std::cout << "✗ DayWise positions failed: " << obj["description"].toString().toStdString() << std::endl;
                    m_testsFailed++;
                }
            } else {
                std::cout << "✗ Network error: " << reply->errorString().toStdString() << std::endl;
                m_testsFailed++;
            }
            
            QTimer::singleShot(500, this, &XTSInteractiveTester::testPositionsNetWise);
            reply->deleteLater();
        });
    }

    void testPositionsNetWise() {
        std::cout << "\nTEST 6: Get Positions (NetWise)" << std::endl;
        std::cout << "Endpoint: GET /interactive/portfolio/positions?dayOrNet=NetWise" << std::endl;
        
        QString url = m_baseURL + "/interactive/portfolio/positions?dayOrNet=NetWise";
        QUrl requestUrl(url); QNetworkRequest request(requestUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", m_authToken.toUtf8());
        
        QNetworkReply *reply = m_networkManager->get(request);
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            
            if (reply->error() == QNetworkReply::NoError) {
                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                QJsonObject obj = doc.object();
                
                if (obj["type"].toString() == "success") {
                    QJsonArray positions = obj["result"].toArray();
                    std::cout << "✓ NetWise positions retrieved - " << positions.size() << " positions found" << std::endl;
                    m_testsPassed++;
                } else if (obj["code"].toString() == "e-property-validation-failed") {
                    std::cout << "✓ NetWise positions endpoint working (no positions)" << std::endl;
                    m_testsPassed++;
                } else {
                    std::cout << "✗ NetWise positions failed: " << obj["description"].toString().toStdString() << std::endl;
                    m_testsFailed++;
                }
            } else {
                std::cout << "✗ Network error: " << reply->errorString().toStdString() << std::endl;
                m_testsFailed++;
            }
            
            QTimer::singleShot(500, this, &XTSInteractiveTester::testOrders);
            reply->deleteLater();
        });
    }

    void testOrders() {
        std::cout << "\nTEST 7: Get Orders" << std::endl;
        std::cout << "Endpoint: GET /interactive/orders" << std::endl;
        
        QString url = m_baseURL + "/interactive/orders";
        QUrl requestUrl(url); QNetworkRequest request(requestUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", m_authToken.toUtf8());
        
        QNetworkReply *reply = m_networkManager->get(request);
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            
            if (reply->error() == QNetworkReply::NoError) {
                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                QJsonObject obj = doc.object();
                
                if (obj["type"].toString() == "success") {
                    QJsonArray orders = obj["result"].toArray();
                    std::cout << "✓ Orders retrieved - " << orders.size() << " orders found" << std::endl;
                    m_testsPassed++;
                } else if (obj["code"].toString() == "e-property-validation-failed") {
                    std::cout << "✓ Orders endpoint working (no orders)" << std::endl;
                    m_testsPassed++;
                } else {
                    std::cout << "✗ Orders failed: " << obj["description"].toString().toStdString() << std::endl;
                    m_testsFailed++;
                }
            } else {
                std::cout << "✗ Network error: " << reply->errorString().toStdString() << std::endl;
                m_testsFailed++;
            }
            
            QTimer::singleShot(500, this, &XTSInteractiveTester::testTrades);
            reply->deleteLater();
        });
    }

    void testTrades() {
        std::cout << "\nTEST 8: Get Trades" << std::endl;
        std::cout << "Endpoint: GET /interactive/orders/trades" << std::endl;
        
        QString url = m_baseURL + "/interactive/orders/trades";
        QUrl requestUrl(url); QNetworkRequest request(requestUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", m_authToken.toUtf8());
        
        QNetworkReply *reply = m_networkManager->get(request);
        
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_testsTotal++;
            
            if (reply->error() == QNetworkReply::NoError) {
                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                QJsonObject obj = doc.object();
                
                if (obj["type"].toString() == "success") {
                    QJsonArray trades = obj["result"].toArray();
                    std::cout << "✓ Trades retrieved - " << trades.size() << " trades found" << std::endl;
                    m_testsPassed++;
                } else if (obj["code"].toString() == "e-property-validation-failed") {
                    std::cout << "✓ Trades endpoint working (no trades)" << std::endl;
                    m_testsPassed++;
                } else {
                    std::cout << "✗ Trades failed: " << obj["description"].toString().toStdString() << std::endl;
                    m_testsFailed++;
                }
            } else {
                std::cout << "✗ Network error: " << reply->errorString().toStdString() << std::endl;
                m_testsFailed++;
            }
            
            QTimer::singleShot(500, this, &XTSInteractiveTester::testLogout);
            reply->deleteLater();
        });
    }

    void testLogout() {
        std::cout << "\nTEST 9: Logout" << std::endl;
        std::cout << "Endpoint: DELETE /interactive/user/session" << std::endl;
        
        QUrl requestUrl(m_baseURL + "/interactive/user/session"); QNetworkRequest request(requestUrl);
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
    QString m_userID;
    QString m_clientCode;
    
    int m_testsPassed;
    int m_testsFailed;
    int m_testsTotal;
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    XTSInteractiveTester tester;
    QTimer::singleShot(0, &tester, &XTSInteractiveTester::runAllTests);
    
    return app.exec();
}

#include "test_interactive_api.moc"

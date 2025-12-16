/**
 * Qt-based API test for XTS getQuote and subscription
 * Uses QNetworkAccessManager (no external dependencies)
 * 
 * Compile: See CMakeLists.txt entry
 * Run: ./build/test_xts_api
 */

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QDebug>
#include <iostream>

class XTSAPITester : public QObject {
    Q_OBJECT
public:
    XTSAPITester(const QString &token, const QString &baseURL, QObject *parent = nullptr)
        : QObject(parent), m_token(token), m_baseURL(baseURL), m_manager(new QNetworkAccessManager(this))
    {
    }
    
    void runTests() {
        qDebug() << "\n================================================";
        qDebug() << "XTS API Test Suite";
        qDebug() << "================================================";
        qDebug() << "Base URL:" << m_baseURL;
        qDebug() << "Token:" << m_token.left(20) + "...";
        qDebug() << "";
        
        // Run tests sequentially with delays
        QTimer::singleShot(100, this, &XTSAPITester::testGetQuote_NIFTY_1502);
        QTimer::singleShot(2000, this, &XTSAPITester::testGetQuote_NIFTY_1501);
        QTimer::singleShot(4000, this, &XTSAPITester::testGetQuote_BANKNIFTY_1502);
        QTimer::singleShot(6000, this, &XTSAPITester::testSubscribe_NIFTY_FirstTime);
        QTimer::singleShot(8000, this, &XTSAPITester::testSubscribe_NIFTY_ReSubscribe);
        QTimer::singleShot(10000, this, &XTSAPITester::testSubscribe_Multiple);
        QTimer::singleShot(12000, this, &XTSAPITester::finishTests);
    }

private slots:
    void testGetQuote_NIFTY_1502() {
        qDebug() << "\n========================================";
        qDebug() << "TEST 1: getQuote - NIFTY - Message Code 1502";
        qDebug() << "========================================";
        
        QJsonObject payload;
        QJsonArray instruments;
        QJsonObject inst;
        inst["exchangeSegment"] = 2;
        inst["exchangeInstrumentID"] = 49543;
        instruments.append(inst);
        payload["instruments"] = instruments;
        payload["xtsMessageCode"] = 1502;
        payload["publishFormat"] = "JSON";  // Required by XTS API
        
        makeRequest("/instruments/quotes", payload, "getQuote-NIFTY-1502");
    }
    
    void testGetQuote_NIFTY_1501() {
        qDebug() << "\n========================================";
        qDebug() << "TEST 2: getQuote - NIFTY - Message Code 1501";
        qDebug() << "========================================";
        
        QJsonObject payload;
        QJsonArray instruments;
        QJsonObject inst;
        inst["exchangeSegment"] = 2;
        inst["exchangeInstrumentID"] = 49543;
        instruments.append(inst);
        payload["instruments"] = instruments;
        payload["xtsMessageCode"] = 1501;
        payload["publishFormat"] = "JSON";  // Required by XTS API
        
        makeRequest("/instruments/quotes", payload, "getQuote-NIFTY-1501");
    }
    
    void testGetQuote_BANKNIFTY_1502() {
        qDebug() << "\n========================================";
        qDebug() << "TEST 3: getQuote - BANKNIFTY - Message Code 1502";
        qDebug() << "========================================";
        
        QJsonObject payload;
        QJsonArray instruments;
        QJsonObject inst;
        inst["exchangeSegment"] = 2;
        inst["exchangeInstrumentID"] = 59175;
        instruments.append(inst);
        payload["instruments"] = instruments;
        payload["xtsMessageCode"] = 1502;
        payload["publishFormat"] = "JSON";  // Required by XTS API
        
        makeRequest("/instruments/quotes", payload, "getQuote-BANKNIFTY-1502");
    }
    
    void testSubscribe_NIFTY_FirstTime() {
        qDebug() << "\n========================================";
        qDebug() << "TEST 4: Subscribe - NIFTY (First Time)";
        qDebug() << "Expected: Should return listQuotes with snapshot";
        qDebug() << "========================================";
        
        QJsonObject payload;
        QJsonArray instruments;
        QJsonObject inst;
        inst["exchangeSegment"] = 2;
        inst["exchangeInstrumentID"] = 49543;
        instruments.append(inst);
        payload["instruments"] = instruments;
        payload["xtsMessageCode"] = 1502;
        
        makeRequest("/instruments/subscription", payload, "Subscribe-NIFTY-First");
    }
    
    void testSubscribe_NIFTY_ReSubscribe() {
        qDebug() << "\n========================================";
        qDebug() << "TEST 5: Subscribe - NIFTY (Re-subscribe)";
        qDebug() << "Expected: Should return success but empty listQuotes";
        qDebug() << "========================================";
        
        QJsonObject payload;
        QJsonArray instruments;
        QJsonObject inst;
        inst["exchangeSegment"] = 2;
        inst["exchangeInstrumentID"] = 49543;
        instruments.append(inst);
        payload["instruments"] = instruments;
        payload["xtsMessageCode"] = 1502;
        
        makeRequest("/instruments/subscription", payload, "Subscribe-NIFTY-ReSubscribe");
    }
    
    void testSubscribe_Multiple() {
        qDebug() << "\n========================================";
        qDebug() << "TEST 6: Subscribe - Multiple Instruments";
        qDebug() << "========================================";
        
        QJsonObject payload;
        QJsonArray instruments;
        
        QJsonObject nifty;
        nifty["exchangeSegment"] = 2;
        nifty["exchangeInstrumentID"] = 49543;
        instruments.append(nifty);
        
        QJsonObject banknifty;
        banknifty["exchangeSegment"] = 2;
        banknifty["exchangeInstrumentID"] = 59175;
        instruments.append(banknifty);
        
        payload["instruments"] = instruments;
        payload["xtsMessageCode"] = 1502;
        
        makeRequest("/instruments/subscription", payload, "Subscribe-Multiple");
    }
    
    void finishTests() {
        qDebug() << "\n================================================";
        qDebug() << "All tests completed!";
        qDebug() << "================================================";
        qDebug() << "";
        qDebug() << "KEY OBSERVATIONS:";
        qDebug() << "1. Check HTTP status codes (200=success, 400=bad request, 404=not found)";
        qDebug() << "2. Does getQuote endpoint work or return errors?";
        qDebug() << "3. Does first subscription return 'listQuotes' array?";
        qDebug() << "4. Does re-subscription return empty 'listQuotes'?";
        qDebug() << "5. What's the structure of touchline data?";
        qDebug() << "";
        
        QTimer::singleShot(1000, qApp, &QCoreApplication::quit);
    }

private:
    void makeRequest(const QString &endpoint, const QJsonObject &payload, const QString &testName) {
        QUrl url(m_baseURL + endpoint);
        QNetworkRequest request(url);
        
        request.setRawHeader("Authorization", m_token.toUtf8());
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        
        QJsonDocument doc(payload);
        QByteArray data = doc.toJson(QJsonDocument::Compact);
        
        qDebug() << "URL:" << url.toString();
        qDebug() << "Request Body:" << QString::fromUtf8(data);
        qDebug() << "";
        
        QNetworkReply *reply = m_manager->post(request, data);
        
        connect(reply, &QNetworkReply::finished, this, [reply, testName, endpoint]() {
            reply->deleteLater();
            
            int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QString httpStatusText = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
            
            qDebug() << "----------------------------------------";
            qDebug() << "Response for:" << testName;
            qDebug() << "HTTP Status:" << httpStatus << httpStatusText;
            
            if (reply->error() != QNetworkReply::NoError) {
                qDebug() << "Error:" << reply->errorString();
            }
            
            QByteArray response = reply->readAll();
            qDebug() << "Response Body:" << QString::fromUtf8(response);
            
            // Try to parse as JSON and pretty print
            QJsonDocument doc = QJsonDocument::fromJson(response);
            if (!doc.isNull()) {
                if (doc.isObject()) {
                    QJsonObject obj = doc.object();
                    
                    qDebug() << "\nParsed Response:";
                    qDebug() << "  type:" << obj["type"].toString();
                    qDebug() << "  description:" << obj["description"].toString();
                    
                    if (obj.contains("result")) {
                        QJsonObject result = obj["result"].toObject();
                        
                        if (result.contains("listQuotes")) {
                            QJsonArray listQuotes = result["listQuotes"].toArray();
                            qDebug() << "  listQuotes count:" << listQuotes.size();
                            
                            if (listQuotes.size() > 0) {
                                QString firstQuote = listQuotes[0].toString();
                                qDebug() << "  First quote (truncated):" << firstQuote.left(200) + "...";
                                
                                // Try to parse the nested JSON
                                QJsonDocument quoteDoc = QJsonDocument::fromJson(firstQuote.toUtf8());
                                if (!quoteDoc.isNull() && quoteDoc.isObject()) {
                                    QJsonObject quoteObj = quoteDoc.object();
                                    if (quoteObj.contains("Touchline")) {
                                        QJsonObject touchline = quoteObj["Touchline"].toObject();
                                        qDebug() << "  Touchline.LastTradedPrice:" << touchline["LastTradedPrice"].toDouble();
                                        qDebug() << "  Touchline.Close:" << touchline["Close"].toDouble();
                                        qDebug() << "  Touchline.Volume:" << touchline["TotalTradedQuantity"].toVariant();
                                    }
                                }
                            } else {
                                qDebug() << "  ⚠️ listQuotes is EMPTY (expected for re-subscription)";
                            }
                        }
                    }
                }
            }
            qDebug() << "----------------------------------------\n";
        });
    }
    
    QString m_token;
    QString m_baseURL;
    QNetworkAccessManager *m_manager;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <auth_token> <base_url>" << std::endl;
        std::cerr << "Example: " << argv[0] << " \"eyJhbGc...\" \"http://192.168.102.9:3000/apimarketdata\"" << std::endl;
        return 1;
    }
    
    QString token = argv[1];
    QString baseURL = argv[2];
    
    XTSAPITester tester(token, baseURL);
    tester.runTests();
    
    return app.exec();
}

#include "test_xts_api.moc"

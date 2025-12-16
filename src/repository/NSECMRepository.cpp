#include "repository/NSECMRepository.h"
#include "repository/MasterFileParser.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

NSECMRepository::NSECMRepository()
    : m_contractCount(0)
    , m_loaded(false)
{
}

NSECMRepository::~NSECMRepository() = default;

void NSECMRepository::allocateArrays(int32_t count) {
    // Token tracking
    m_token.resize(count);
    
    // Security Master Data (4 fields)
    m_name.resize(count);
    m_displayName.resize(count);
    m_description.resize(count);
    m_series.resize(count);
    
    // Trading Parameters (3 fields)
    m_lotSize.resize(count);
    m_tickSize.resize(count);
    m_freezeQty.resize(count);
    
    // Price Bands (2 fields)
    m_priceBandHigh.resize(count);
    m_priceBandLow.resize(count);
    
    // Live Market Data (7 fields)
    m_ltp.resize(count);
    m_open.resize(count);
    m_high.resize(count);
    m_low.resize(count);
    m_close.resize(count);
    m_prevClose.resize(count);
    m_volume.resize(count);
    
    // WebSocket-only fields
    m_bidPrice.resize(count);
    m_askPrice.resize(count);
}

bool NSECMRepository::loadMasterFile(const QString& filename) {
    QWriteLocker locker(&m_mutex);
    
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open NSECM master file:" << filename;
        return false;
    }
    
    // First pass: count contracts
    QTextStream in(&file);
    QVector<MasterContract> contracts;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        
        MasterContract contract;
        if (MasterFileParser::parseLine(line, "NSECM", contract)) {
            contracts.append(contract);
        }
    }
    
    file.close();
    
    // Allocate arrays
    int32_t count = contracts.size();
    allocateArrays(count);
    m_tokenToIndex.reserve(count);
    
    // Second pass: load data
    for (int32_t idx = 0; idx < count; ++idx) {
        const MasterContract& contract = contracts[idx];
        int64_t token = contract.exchangeInstrumentID;
        
        m_tokenToIndex[token] = idx;
        m_token[idx] = token;
        m_name[idx] = contract.name;
        m_displayName[idx] = contract.displayName;
        m_description[idx] = contract.description;
        m_series[idx] = contract.series;
        m_lotSize[idx] = contract.lotSize;
        m_tickSize[idx] = contract.tickSize;
        m_freezeQty[idx] = contract.freezeQty;
        m_priceBandHigh[idx] = contract.priceBandHigh;
        m_priceBandLow[idx] = contract.priceBandLow;
        
        // Initialize live data to zero
        m_ltp[idx] = 0.0;
        m_open[idx] = 0.0;
        m_high[idx] = 0.0;
        m_low[idx] = 0.0;
        m_close[idx] = 0.0;
        m_prevClose[idx] = 0.0;
        m_volume[idx] = 0;
        m_bidPrice[idx] = 0.0;
        m_askPrice[idx] = 0.0;
    }
    
    m_contractCount = count;
    m_loaded = true;
    
    qDebug() << "NSE CM Repository loaded:" << m_contractCount << "contracts";
    
    return true;
}

bool NSECMRepository::loadProcessedCSV(const QString& filename) {
    QWriteLocker locker(&m_mutex);
    
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open NSECM CSV file:" << filename;
        return false;
    }
    
    QTextStream in(&file);
    
    // Skip header line
    if (!in.atEnd()) {
        in.readLine();
    }
    
    QVector<QStringList> records;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        
        QStringList fields = line.split(',');
        if (fields.size() >= 17) {  // NSECM CSV has 17 columns
            records.append(fields);
        }
    }
    
    file.close();
    
    // Allocate arrays
    int32_t count = records.size();
    allocateArrays(count);
    m_tokenToIndex.reserve(count);
    
    // Load data
    for (int32_t idx = 0; idx < count; ++idx) {
        const QStringList& fields = records[idx];
        
        bool ok;
        int64_t token = fields[0].toLongLong(&ok);
        if (!ok) {
            continue;
        }
        
        m_tokenToIndex[token] = idx;
        m_token[idx] = token;
        m_name[idx] = fields[1];             // Symbol
        m_displayName[idx] = fields[2];      // DisplayName
        m_description[idx] = fields[3];      // Description
        m_series[idx] = fields[4];           // Series
        m_lotSize[idx] = fields[5].toInt();
        m_tickSize[idx] = fields[6].toDouble();
        // fields[7] = ISIN (not stored separately)
        m_priceBandHigh[idx] = fields[8].toDouble();
        m_priceBandLow[idx] = fields[9].toDouble();
        
        // Live market data (initialized to 0 in CSV)
        m_ltp[idx] = fields[10].toDouble();
        m_open[idx] = fields[11].toDouble();
        m_high[idx] = fields[12].toDouble();
        m_low[idx] = fields[13].toDouble();
        m_close[idx] = fields[14].toDouble();
        m_prevClose[idx] = fields[15].toDouble();
        m_volume[idx] = fields[16].toLongLong();
        
        // Initialize WebSocket-only fields
        m_bidPrice[idx] = 0.0;
        m_askPrice[idx] = 0.0;
        m_freezeQty[idx] = 0;  // Not in CSV
    }
    
    m_contractCount = count;
    m_loaded = true;
    
    qDebug() << "NSE CM Repository loaded from CSV:" << m_contractCount << "contracts";
    
    return true;
}

int32_t NSECMRepository::getIndex(int64_t token) const {
    auto it = m_tokenToIndex.find(token);
    if (it != m_tokenToIndex.end()) {
        return it.value();
    }
    return -1;
}

const ContractData* NSECMRepository::getContract(int64_t token) const {
    QReadLocker locker(&m_mutex);
    
    int32_t idx = getIndex(token);
    if (idx < 0) {
        return nullptr;
    }
    
    // Create temporary ContractData from arrays
    static thread_local ContractData tempContract;
    tempContract.exchangeInstrumentID = token;
    tempContract.name = m_name[idx];
    tempContract.displayName = m_displayName[idx];
    tempContract.description = m_description[idx];
    tempContract.series = m_series[idx];
    tempContract.lotSize = m_lotSize[idx];
    tempContract.tickSize = m_tickSize[idx];
    tempContract.freezeQty = m_freezeQty[idx];
    tempContract.priceBandHigh = m_priceBandHigh[idx];
    tempContract.priceBandLow = m_priceBandLow[idx];
    tempContract.ltp = m_ltp[idx];
    tempContract.open = m_open[idx];
    tempContract.high = m_high[idx];
    tempContract.low = m_low[idx];
    tempContract.close = m_close[idx];
    tempContract.prevClose = m_prevClose[idx];
    tempContract.volume = m_volume[idx];
    tempContract.bidPrice = m_bidPrice[idx];
    tempContract.askPrice = m_askPrice[idx];
    
    return &tempContract;
}

bool NSECMRepository::hasContract(int64_t token) const {
    QReadLocker locker(&m_mutex);
    return m_tokenToIndex.contains(token);
}

void NSECMRepository::updateLiveData(int64_t token, double ltp, int64_t volume) {
    QWriteLocker locker(&m_mutex);
    
    int32_t idx = getIndex(token);
    if (idx >= 0) {
        m_ltp[idx] = ltp;
        m_volume[idx] = volume;
    }
}

void NSECMRepository::updateBidAsk(int64_t token, double bidPrice, double askPrice) {
    QWriteLocker locker(&m_mutex);
    
    int32_t idx = getIndex(token);
    if (idx >= 0) {
        m_bidPrice[idx] = bidPrice;
        m_askPrice[idx] = askPrice;
    }
}

void NSECMRepository::updateOHLC(int64_t token, double open, double high, double low, double close) {
    QWriteLocker locker(&m_mutex);
    
    int32_t idx = getIndex(token);
    if (idx >= 0) {
        m_open[idx] = open;
        m_high[idx] = high;
        m_low[idx] = low;
        m_close[idx] = close;
    }
}

QVector<ContractData> NSECMRepository::getAllContracts() const {
    QReadLocker locker(&m_mutex);
    
    QVector<ContractData> contracts;
    contracts.reserve(m_contractCount);
    
    for (int32_t idx = 0; idx < m_contractCount; ++idx) {
        ContractData contract;
        contract.exchangeInstrumentID = m_token[idx];
        contract.name = m_name[idx];
        contract.displayName = m_displayName[idx];
        contract.description = m_description[idx];
        contract.series = m_series[idx];
        contract.lotSize = m_lotSize[idx];
        contract.tickSize = m_tickSize[idx];
        contract.freezeQty = m_freezeQty[idx];
        contract.priceBandHigh = m_priceBandHigh[idx];
        contract.priceBandLow = m_priceBandLow[idx];
        contract.ltp = m_ltp[idx];
        contract.volume = m_volume[idx];
        
        contracts.append(contract);
    }
    
    return contracts;
}

QVector<ContractData> NSECMRepository::getContractsBySeries(const QString& series) const {
    QReadLocker locker(&m_mutex);
    
    QVector<ContractData> contracts;
    
    for (int32_t idx = 0; idx < m_contractCount; ++idx) {
        if (m_series[idx] == series) {
            ContractData contract;
            contract.exchangeInstrumentID = m_token[idx];
            contract.name = m_name[idx];
            contract.displayName = m_displayName[idx];
            contract.series = m_series[idx];
            contract.lotSize = m_lotSize[idx];
            
            contracts.append(contract);
        }
    }
    
    return contracts;
}

QVector<ContractData> NSECMRepository::getContractsBySymbol(const QString& symbol) const {
    QReadLocker locker(&m_mutex);
    
    QVector<ContractData> contracts;
    
    for (int32_t idx = 0; idx < m_contractCount; ++idx) {
        if (m_name[idx] == symbol) {
            ContractData contract;
            contract.exchangeInstrumentID = m_token[idx];
            contract.name = m_name[idx];
            contract.displayName = m_displayName[idx];
            contract.series = m_series[idx];
            contract.lotSize = m_lotSize[idx];
            
            contracts.append(contract);
        }
    }
    
    return contracts;
}

bool NSECMRepository::saveProcessedCSV(const QString& filename) const {
    QReadLocker locker(&m_mutex);
    
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filename;
        return false;
    }
    
    QTextStream out(&file);
    
    // Write header
    out << "Token,Symbol,DisplayName,Description,Series,LotSize,TickSize,PriceBandHigh,PriceBandLow,LTP,Open,High,Low,Close,PrevClose,Volume\n";
    
    // Write all contracts
    for (int32_t idx = 0; idx < m_contractCount; ++idx) {
        out << m_token[idx] << ","
            << m_name[idx] << ","
            << m_displayName[idx] << ","
            << m_description[idx] << ","
            << m_series[idx] << ","
            << m_lotSize[idx] << ","
            << m_tickSize[idx] << ","
            << m_priceBandHigh[idx] << ","
            << m_priceBandLow[idx] << ","
            << "0,0,0,0,0,0,0\n";  // Live data not persisted
    }
    
    file.close();
    qDebug() << "NSE CM Repository saved to CSV:" << filename << m_contractCount << "contracts";
    return true;
}

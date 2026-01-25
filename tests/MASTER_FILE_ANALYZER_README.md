# Master File Analyzer - Build & Run Instructions

## Purpose

This utility analyzes the raw `master_contracts_latest.txt` file and generates a comprehensive markdown report covering:

- **Exchange segment breakdown** (NSEFO, NSECM, BSEFO, BSECM)
- **Asset token patterns** (critical for Greeks calculation)
- **Expiry date formats** (ISO vs DDMMMYYYY)
- **Strike price precision** (fractional vs integer)
- **Token range analysis** (for repository storage strategy)
- **Data quality issues** (validation errors, missing fields)
- **Exchange-specific quirks** (composite tokens, index underlying)

## Quick Start

### Option 1: Build with Main Project (CMake)

Add to your `CMakeLists.txt`:

```cmake
# Master file analyzer utility
add_executable(master_file_analyzer
    tests/master_file_analyzer.cpp
)

target_link_libraries(master_file_analyzer
    Qt5::Core
)

# Set output directory
set_target_properties(master_file_analyzer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
```

Then build:

```bash
cd build
cmake ..
make master_file_analyzer
```

### Option 2: Standalone Compilation (qmake)

Create `master_file_analyzer.pro`:

```pro
QT       += core
QT       -= gui

TARGET = master_file_analyzer
CONFIG   += console c++17
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += master_file_analyzer.cpp
```

Build:

```bash
cd tests
qmake master_file_analyzer.pro
make
```

### Option 3: Direct Compilation (macOS/Linux)

```bash
cd tests

# macOS
g++ -std=c++17 -I/usr/local/opt/qt@5/include \
    -I/usr/local/opt/qt@5/include/QtCore \
    -L/usr/local/opt/qt@5/lib \
    -framework QtCore \
    -o master_file_analyzer master_file_analyzer.cpp

# Linux
g++ -std=c++17 -I/usr/include/qt5 \
    -I/usr/include/qt5/QtCore \
    -L/usr/lib/x86_64-linux-gnu \
    -lQt5Core \
    -o master_file_analyzer master_file_analyzer.cpp
```

## Usage

```bash
./master_file_analyzer <path_to_master_contracts_latest.txt> [output_report.md]
```

### Example:

```bash
# From build directory
./bin/master_file_analyzer \
    ../build/TradingTerminal.app/Contents/MacOS/Masters/master_contracts_latest.txt \
    master_analysis_report.md

# From project root
./build/bin/master_file_analyzer \
    build/TradingTerminal.app/Contents/MacOS/Masters/master_contracts_latest.txt \
    docs/MASTER_FILE_ANALYSIS_REPORT.md
```

## Expected Output

The tool will generate a comprehensive markdown report with these sections:

1. **Executive Summary**
   - Total records, parse errors, unique symbols
   - Key metrics with pass/fail status

2. **Exchange Segment Breakdown**
   - NSEFO, NSECM, BSEFO, BSECM distribution
   - Percentage analysis

3. **Series Distribution**
   - OPTSTK, OPTIDX, FUTSTK, FUTIDX, EQ, INDEX

4. **Token Range Analysis**
   - Min/max tokens per exchange
   - Recommended storage strategy (array vs hash)

5. **Asset Token Analysis** ⚠️ **CRITICAL**
   - Composite token patterns
   - Records with asset_token = -1 (needs index master)
   - Most common asset tokens

6. **Expiry Date Format Analysis**
   - ISO vs DDMMMYYYY distribution
   - Parsing errors and recommendations

7. **Strike Price Analysis**
   - Min/max strikes
   - Fractional precision by symbol
   - Strike distribution

8. **Option Type Distribution**
   - CE vs PE balance check
   - Top symbols by option count

9. **Exchange-Specific Quirks**
   - Composite tokens usage
   - Index underlying patterns
   - Fractional strikes

10. **Data Quality Issues**
    - Validation errors
    - Error categories
    - First 20 errors for debugging

11. **Implementation Recommendations**
    - Asset token extraction code
    - Expiry date parsing code
    - Repository storage strategy
    - Index master integration steps

## Performance

- **Speed:** ~10,000 records/second
- **Memory:** ~200MB peak for 130,000+ records
- **Time:** 10-15 seconds for full analysis

## Sample Console Output

```
Starting analysis...
Input file: build/TradingTerminal.app/Contents/MacOS/Masters/master_contracts_latest.txt
Output file: master_analysis_report.md
Processed 10000 lines...
Processed 20000 lines...
Processed 30000 lines...
...
Processed 130000 lines...
Analysis complete. Analyzing exchange quirks...
Generating report...
Report generated: master_analysis_report.md
Done!
Total records: 131009
Parse errors: 0
Report saved to: master_analysis_report.md
```

## Troubleshooting

### Issue: Qt5 not found

**Solution (macOS with Homebrew):**
```bash
export Qt5_DIR=/usr/local/opt/qt@5/lib/cmake/Qt5
# or
brew install qt@5
brew link qt@5 --force
```

**Solution (Linux - Ubuntu/Debian):**
```bash
sudo apt-get install qtbase5-dev
```

### Issue: Parse errors in output

**Cause:** Master file has unexpected format changes

**Action:**
1. Check the "Data Quality Issues" section in the report
2. Look at "First 20 Errors" for specific line numbers
3. Manually inspect those lines in the master file
4. Update the parser logic if needed

### Issue: Report shows 0 records

**Cause:** File encoding or line ending issues

**Solution:**
```bash
# Check file format
file master_contracts_latest.txt

# Convert if needed (DOS to Unix)
dos2unix master_contracts_latest.txt

# Check line count
wc -l master_contracts_latest.txt
```

## Key Findings to Look For

### 1. Asset Token Patterns

Look for this in the report:

```markdown
| Pattern | Count | Note |
|---------|-------|------|
| COMPOSITE (Prefix: 11001000, Extracted: 2885) | 75000 | ✅ |
| MINUS_ONE (Index underlying - needs mapping) | 15000 | ❌ **Needs index master mapping** |
| DIRECT (26000) | 270 | ✅ |
```

**Action:** If "MINUS_ONE" count > 0, you MUST load index master first!

### 2. Expiry Date Format

Look for this:

```markdown
| Format | Count | Percentage | Recommended Parser |
|--------|-------|------------|-------------------|
| ISO_DATETIME | 105000 | 99.5% | QDateTime::fromString(expiry, Qt::ISODate) |
| DDMMMYYYY | 500 | 0.5% | QDate::fromString(expiry, "ddMMMyyyy") |
```

**Action:** Use ISO parser as primary, DDMMMYYYY as fallback

### 3. Token Range Strategy

Look for this:

```markdown
| Exchange | Min Token | Max Token | Range | Recommended Storage |
|----------|-----------|-----------|-------|---------------------|
| NSEFO | 35000 | 199950 | 164950 | Array (indexed) |
| NSECM | 1 | 99999 | 99998 | Array (indexed) |
```

**Action:** Use array-based storage for dense token ranges

## Next Steps After Running

1. **Review the generated report** - Look for ❌ marks (critical issues)

2. **Check Section 5 (Asset Token Analysis)** - If "MINUS_ONE" count > 0:
   - Load `nse_cm_index_master.csv` BEFORE parsing NSEFO
   - Build symbol → token mapping
   - Update asset tokens during NSEFO parsing

3. **Check Section 6 (Expiry Date Analysis)** - If parsing errors > 0:
   - Review error patterns
   - Update `MasterFileParser.cpp` to handle those formats

4. **Check Section 10 (Data Quality Issues)** - If validation errors > 0:
   - Review first 20 errors
   - Add validation logic to parser

5. **Implement recommendations from Section 11** - Copy/paste code snippets into your parser

## Integration with Main Codebase

Once you've reviewed the report, update these files:

### 1. `MasterFileParser.cpp`

Use the recommended expiry date parser from the report:

```cpp
// From report Section 11
QDateTime expiryDateTime = QDateTime::fromString(expiryStr, Qt::ISODate);
if (expiryDateTime.isValid()) {
    contract.expiryDate_dt = expiryDateTime.date();
    contract.expiryDate = expiryDateTime.date().toString("ddMMMyyyy").toUpper();
}
```

### 2. `RepositoryManager.cpp`

Use the asset token extraction logic from the report:

```cpp
// From report Section 11
int64_t extractAssetToken(int64_t rawToken, const QString& symbol) {
    if (rawToken == -1) {
        // Index option - lookup from index master
        return m_indexNameTokenMap.value(symbol, 0);
    }
    
    if (rawToken > 10000000000LL) {
        // Composite format: extract last 5 digits
        return rawToken % 100000;
    }
    
    return rawToken;
}
```

### 3. `NSEFORepository.cpp` / `NSECMRepository.cpp`

Use the recommended storage strategy from the report.

## Conclusion

This analyzer tool provides **ground truth** about your actual master file format. Use it to:

✅ **Validate assumptions** about data formats  
✅ **Identify critical issues** (missing asset tokens)  
✅ **Make data-driven decisions** about storage strategies  
✅ **Generate precise parser code** based on real data  

**Run this tool BEFORE making any changes to your parser!**

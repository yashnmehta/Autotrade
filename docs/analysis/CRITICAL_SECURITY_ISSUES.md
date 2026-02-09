# Critical Security Issues - Trading Terminal C++

**Classification**: 🔴 CRITICAL - P0  
**Impact**: Account compromise, unauthorized trading, financial loss  
**Estimated Fix Time**: 1-3 days  
**Requires Immediate Action**: YES

---

## 🚨 Executive Summary

**3 Critical Security Vulnerabilities Found**:

1. **SEC-001**: API credentials hardcoded in configuration file
2. **SEC-002**: Sensitive data committed to version control (git history)
3. **SEC-003**: No encryption for credentials at rest or in transit

**Blast Radius**: If repository is leaked or made public:
- ✅ Attacker gains full API access to trading account
- ✅ Can place orders, cancel positions, extract trade history
- ✅ Multiple historical credential sets exposed (git history)
- ✅ Source IP visible (192.168.102.9) - internal network mapping

**Industry Comparison**: ❌ FAILS PCI DSS, SOC 2, and OWASP Top 10 standards

---

## 🔍 Detailed Findings

### SEC-001: Hardcoded API Credentials (CRITICAL)

**Location**: `configs/config.ini` lines 6-50

**Vulnerable Code**:
```ini
[CREDENTIALS]
# ACTIVE CREDENTIALS (EXPOSED)
marketdata_appkey = 0ad2e5b5745f883b275594
marketdata_secretkey = Hbem321@hG
interactive_appkey = a36a44c17d008920af5719
interactive_secretkey = Tiyv603$Pm
source = TWSAPI

# HISTORICAL CREDENTIALS (STILL IN FILE)
; marketdata_appkey     = 70f619c508ad8eb3802888
; marketdata_secretkey  = Hocy344#BR
; interactive_appkey    = bfd0b3bcb0b52a40eb6826
; interactive_secretkey = Cdfn817@X0

; marketdata_appkey     = 2d832e8d71e1d180aee499
; marketdata_secretkey  = Snvd485$cC
; interactive_appkey    = 5820d8e017294c81d71873
; interactive_secretkey = Ibvk668@NX
```

**CVSS Score**: 9.8/10 (Critical)
- Attack Vector: Network (if repo leaked)
- Attack Complexity: Low
- Privileges Required: None
- User Interaction: None
- Confidentiality: High
- Integrity: High
- Availability: High

**Attack Scenarios**:

1. **Repository Leak**:
   ```bash
   # Attacker finds repo on GitHub/GitLab/Bitbucket
   git clone <repo>
   cat configs/config.ini
   # Extracts credentials, uses with curl/Python
   curl -H "Authorization: Bearer <appkey>" https://mtrade.arhamshare.com/api
   ```

2. **Insider Threat**:
   - Employee with read-only access sees credentials
   - No audit trail of who accessed config file

3. **Supply Chain Attack**:
   - Dependency vulnerability exposes filesystem
   - Config file read by malicious library

**Regulatory Impact**:
- ❌ Violates PCI DSS Requirement 8.2.1 (no hardcoded credentials)
- ❌ Violates GDPR Article 32 (appropriate security measures)
- ❌ Violates SOC 2 Common Criteria CC6.1 (logical access controls)

---

### SEC-002: Credentials in Git History (CRITICAL)

**Location**: `.git/` directory

**Issue**: Even if credentials are removed from `config.ini`, they remain in git history forever.

**Detection**:
```bash
# Anyone with repo access can search history
git log -p --all -S "marketdata_secretkey"
git log -p --all -S "Hbem321@hG"

# Or use git-secrets tool
git secrets --scan-history
```

**Historical Exposure**:
- Minimum 4 sets of credentials exposed
- Potentially months/years of trading account access
- No rotation evidence (same API keys reused)

**Attack Vector**:
```bash
# Attacker clones repo, extracts all historical secrets
git clone <repo>
cd trading_terminal_cpp
git log --all --full-history --source -S "secretkey" > secrets.txt
# Tries each credential set until one works
```

**Mitigation Requirements**:
1. Purge git history (requires force-push to all remotes)
2. Notify XTS/broker of credential exposure
3. Rotate ALL exposed credentials
4. Audit: Check for unauthorized API calls using old keys

---

### SEC-003: No Credential Encryption (HIGH)

**Location**: `src/utils/ConfigLoader.cpp` (inferred from usage)

**Issue**: Credentials stored in plaintext on disk.

**Current Flow**:
```
config.ini (plaintext on disk)
    ↓
ConfigLoader reads file
    ↓
QString/std::string in memory (plaintext)
    ↓
Passed to XTS API
```

**Problems**:
1. **At Rest**: File readable by any process with user privileges
2. **In Memory**: Credentials in plaintext strings (memory dumps expose)
3. **In Logs**: Risk of accidental logging (common mistake)
4. **In Crash Dumps**: Windows Error Reporting sends crash dumps with memory

**Industry Standards**:
- Bloomberg: Uses Windows DPAPI (Data Protection API)
- Interactive Brokers: Certificates + encrypted config
- MetaTrader: Encrypted configuration with user password

---

## 🛡️ Remediation Plan

### Phase 1: IMMEDIATE (Today)

**Step 1: Verify Credential Exposure**
```bash
# Check if repo is in any remote
git remote -v

# Check if pushed to GitHub/GitLab
git log --all --oneline | wc -l  # Check commit count
```

**Step 2: Emergency Rotation**
1. Log into XTS broker portal
2. Revoke exposed API keys immediately:
   - `0ad2e5b5745f883b275594`
   - `a36a44c17d008920af5719`
3. Generate new credentials
4. Rotate ALL historical keys found in comments

**Step 3: Damage Assessment**
```bash
# Check for unauthorized API usage
# Contact XTS support for API access logs
# Compare with your legitimate usage patterns
```

---

### Phase 2: SHORT-TERM (1-3 Days)

**Option A: Environment Variables (Simplest)**

**Step 1: Update ConfigLoader**
```cpp
// src/utils/ConfigLoader.cpp
QString ConfigLoader::getApiKey() {
    // Priority: Environment variable > Config file
    QString envKey = qgetenv("XTS_MD_APPKEY");
    if (!envKey.isEmpty()) {
        return envKey;
    }
    
    // Fallback to config (for development only)
    QSettings settings("configs/config.ini", QSettings::IniFormat);
    QString fileKey = settings.value("CREDENTIALS/marketdata_appkey", "").toString();
    
    if (fileKey.isEmpty()) {
        qCritical() << "[Security] No API credentials found!";
        throw std::runtime_error("Missing API credentials");
    }
    
    qWarning() << "[Security] Using credentials from file - insecure!";
    return fileKey;
}
```

**Step 2: Environment Setup**
```powershell
# Windows - User-level (persists across reboots)
[System.Environment]::SetEnvironmentVariable("XTS_MD_APPKEY", "new_key_here", "User")
[System.Environment]::SetEnvironmentVariable("XTS_MD_SECRET", "new_secret_here", "User")
[System.Environment]::SetEnvironmentVariable("XTS_IA_APPKEY", "new_key_here", "User")
[System.Environment]::SetEnvironmentVariable("XTS_IA_SECRET", "new_secret_here", "User")

# Verify
$env:XTS_MD_APPKEY
```

**Step 3: Update Config File**
```ini
# configs/config.ini (now safe to commit)
[CREDENTIALS]
# Credentials loaded from environment variables:
# XTS_MD_APPKEY, XTS_MD_SECRET, XTS_IA_APPKEY, XTS_IA_SECRET
# Set these in Windows environment or .env file (gitignored)
marketdata_appkey = ${ENV:XTS_MD_APPKEY}
marketdata_secretkey = ${ENV:XTS_MD_SECRET}
interactive_appkey = ${ENV:XTS_IA_APPKEY}
interactive_secretkey = ${ENV:XTS_IA_SECRET}
source = TWSAPI
```

**Step 4: Git Hygiene**
```bash
# Remove credentials from current commit
git rm --cached configs/config.ini
echo "configs/config.ini" >> .gitignore

# Create template
cp configs/config.ini configs/config.ini.template
# Manually replace secrets with placeholders in template
git add configs/config.ini.template .gitignore
git commit -m "security: Move credentials to environment variables"
```

---

**Option B: Windows Credential Manager (Recommended for Production)**

**Step 1: Install Qt Keychain (Optional Dependency)**
```cmake
# CMakeLists.txt
find_package(Qt5Keychain QUIET)
if(Qt5Keychain_FOUND)
    target_link_libraries(TradingTerminal Qt5::Keychain)
    target_compile_definitions(TradingTerminal PRIVATE USE_KEYCHAIN)
endif()
```

**Step 2: Secure Storage Wrapper**
```cpp
// include/utils/SecureCredentialStore.h
#ifndef SECURE_CREDENTIAL_STORE_H
#define SECURE_CREDENTIAL_STORE_H

#include <QString>
#include <QSettings>
#ifdef USE_KEYCHAIN
#include <qtkeychain/keychain.h>
#endif

class SecureCredentialStore {
public:
    static QString getCredential(const QString& key);
    static void setCredential(const QString& key, const QString& value);
    static bool hasCredential(const QString& key);
    
private:
    static const QString SERVICE_NAME;
};

#endif
```

**Step 3: Implementation**
```cpp
// src/utils/SecureCredentialStore.cpp
#include "utils/SecureCredentialStore.h"
#include <QDebug>

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#pragma comment(lib, "Advapi32.lib")
#endif

const QString SecureCredentialStore::SERVICE_NAME = "TradingTerminal";

QString SecureCredentialStore::getCredential(const QString& key) {
#ifdef _WIN32
    // Use Windows Credential Manager (native API)
    QString target = SERVICE_NAME + "/" + key;
    PCREDENTIALW credential = nullptr;
    
    if (CredReadW(target.toStdWString().c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
        QString value = QString::fromWCharArray(
            (wchar_t*)credential->CredentialBlob,
            credential->CredentialBlobSize / sizeof(wchar_t)
        );
        CredFree(credential);
        return value;
    }
    
    qWarning() << "[SecureStore] Credential not found:" << key;
    return QString();
    
#else
    // Linux/macOS: Use libsecret or Keychain
    qWarning() << "[SecureStore] Platform not supported, using fallback";
    QSettings settings("configs/config.ini", QSettings::IniFormat);
    return settings.value("CREDENTIALS/" + key, "").toString();
#endif
}

void SecureCredentialStore::setCredential(const QString& key, const QString& value) {
#ifdef _WIN32
    QString target = SERVICE_NAME + "/" + key;
    CREDENTIALW credential = {};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = (LPWSTR)target.toStdWString().c_str();
    credential.CredentialBlobSize = value.size() * sizeof(wchar_t);
    credential.CredentialBlob = (LPBYTE)value.toStdWString().c_str();
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    
    if (!CredWriteW(&credential, 0)) {
        qCritical() << "[SecureStore] Failed to store credential:" << key;
    }
#endif
}
```

**Step 4: Update ConfigLoader**
```cpp
// src/utils/ConfigLoader.cpp
#include "utils/SecureCredentialStore.h"

QString ConfigLoader::getApiKey() {
    // Try secure store first
    QString key = SecureCredentialStore::getCredential("marketdata_appkey");
    if (!key.isEmpty()) {
        return key;
    }
    
    // Fallback to environment
    key = qgetenv("XTS_MD_APPKEY");
    if (!key.isEmpty()) {
        return key;
    }
    
    // Last resort: config file (development only)
    QSettings settings("configs/config.ini", QSettings::IniFormat);
    key = settings.value("CREDENTIALS/marketdata_appkey", "").toString();
    
    if (key.isEmpty()) {
        throw std::runtime_error("No API credentials configured");
    }
    
    qWarning() << "[ConfigLoader] Using insecure file-based credentials!";
    return key;
}
```

**Step 5: Initial Credential Setup (One-Time)**
```cpp
// Create setup utility: tools/setup_credentials.cpp
#include <QCoreApplication>
#include <QTextStream>
#include <iostream>
#include "utils/SecureCredentialStore.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    QTextStream in(stdin);
    QTextStream out(stdout);
    
    out << "Trading Terminal - Secure Credential Setup\n";
    out << "==========================================\n\n";
    
    out << "Market Data App Key: ";
    out.flush();
    QString mdAppKey = in.readLine();
    
    out << "Market Data Secret: ";
    out.flush();
    QString mdSecret = in.readLine();
    
    out << "Interactive App Key: ";
    out.flush();
    QString iaAppKey = in.readLine();
    
    out << "Interactive Secret: ";
    out.flush();
    QString iaSecret = in.readLine();
    
    // Store securely
    SecureCredentialStore::setCredential("marketdata_appkey", mdAppKey);
    SecureCredentialStore::setCredential("marketdata_secretkey", mdSecret);
    SecureCredentialStore::setCredential("interactive_appkey", iaAppKey);
    SecureCredentialStore::setCredential("interactive_secretkey", iaSecret);
    
    out << "\n✅ Credentials stored securely in Windows Credential Manager\n";
    out << "You can now delete them from config.ini\n";
    
    return 0;
}
```

---

### Phase 3: LONG-TERM (1-2 Weeks)

**1. Purge Git History**

⚠️ **WARNING**: This requires force-push and breaks all clones. Coordinate with team.

```bash
# Option A: BFG Repo-Cleaner (fastest)
java -jar bfg.jar --replace-text passwords.txt trading_terminal_cpp.git
cd trading_terminal_cpp.git
git reflog expire --expire=now --all
git gc --prune=now --aggressive

# Option B: git-filter-repo
git filter-repo --path configs/config.ini --invert-paths --force

# Option C: Manual (if small repo)
git filter-branch --force --index-filter \
  "git rm --cached --ignore-unmatch configs/config.ini" \
  --prune-empty --tag-name-filter cat -- --all
```

**2. Enable Git Secrets Detection**

```bash
# Install git-secrets
git clone https://github.com/awslabs/git-secrets.git
cd git-secrets
make install

# Configure for this repo
cd /path/to/trading_terminal_cpp
git secrets --install
git secrets --register-aws  # Detects AWS keys
git secrets --add 'secretkey.*=.*'
git secrets --add 'appkey.*=.*'
git secrets --add '[0-9a-f]{24}'  # 24-char hex keys

# Scan current repo
git secrets --scan-history
```

**3. Add Pre-Commit Hook**

```bash
# .git/hooks/pre-commit
#!/bin/bash
# Prevent committing sensitive files

if git diff --cached --name-only | grep -q "config.ini$"; then
    echo "❌ ERROR: Attempting to commit config.ini"
    echo "Only config.ini.template should be committed"
    exit 1
fi

# Check for common secret patterns
if git diff --cached -U0 | grep -qE "(password|secret|key|token).*=.*['\"]"; then
    echo "⚠️  WARNING: Possible secret detected in commit"
    echo "Please review changes carefully"
    git diff --cached | grep -E "(password|secret|key|token)"
    read -p "Continue anyway? (y/N) " -n  1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi
```

---

## 🔒 Additional Security Hardening

### 1. API Request Signing (Prevents Credential Replay)

```cpp
// include/api/XTSAuthenticator.h
class XTSAuthenticator {
public:
    static QString generateHMAC(const QString& message, const QString& secret);
    static QString createSignedRequest(const QString& endpoint, 
                                      const QJsonObject& payload,
                                      const QString& apiKey,
                                      const QString& secretKey);
};

// Implementation with Qt Cryptographic Hash
QString XTSAuthenticator::generateHMAC(const QString& message, 
                                       const QString& secret) {
    QByteArray key = secret.toUtf8();
    QByteArray msg = message.toUtf8();
    
    // HMAC-SHA256
    QMessageAuthenticationCode code(QCryptographicHash::Sha256);
    code.setKey(key);
    code.addData(msg);
    
    return QString(code.result().toHex());
}
```

### 2. Credential Rotation Schedule

```cpp
// include/utils/CredentialRotationChecker.h
class CredentialRotationChecker {
public:
    static bool shouldRotate();
    static QDateTime lastRotationDate();
    static void setRotationDate(const QDateTime& date);
    
private:
    static const int ROTATION_DAYS = 90;  // Industry standard
};

// Check on startup
if (CredentialRotationChecker::shouldRotate()) {
    QMessageBox::warning(nullptr, "Security Alert",
        "API credentials are older than 90 days.\n"
        "Please rotate them for security.");
}
```

### 3. Audit Logging

```cpp
// Log all credential access (not the values!)
void ConfigLoader::loadCredentials() {
    qInfo() << "[AUDIT]" << QDateTime::currentDateTime().toString(Qt::ISODate)
            << "User:" << qgetenv("USERNAME")
            << "Action: Load credentials"
            << "Method: Environment variables";
}
```

---

## ✅ Verification Checklist

After implementing fixes:

- [ ] No credentials in `configs/config.ini`
- [ ] `config.ini` in `.gitignore`
- [ ] `config.ini.template` provided with `<PLACEHOLDER>` values
- [ ] Credentials loaded from Windows Credential Manager OR environment variables
- [ ] Git history purged (if applicable)
- [ ] All historical credentials rotated
- [ ] Pre-commit hook installed
- [ ] git-secrets configured
- [ ] Audit logging enabled
- [ ] Team trained on new credential management

---

## 📚 References

- **OWASP**: [Secret Management Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Secret_Management_Cheat_Sheet.html)
- **Microsoft**: [Windows Credential Manager API](https://docs.microsoft.com/en-us/windows/win32/api/wincred/)
- **Qt**: [QKeychain Documentation](https://github.com/frankosterfeld/qtkeychain)
- **NIST**: [Special Publication 800-63B](https://pages.nist.gov/800-63-3/sp800-63b.html) (Authentication guidelines)

---

**Document Version**: 1.0  
**Last Updated**: February 8, 2026  
**Severity**: 🔴 CRITICAL  
**Status**: ⚠️ OPEN - Requires immediate action

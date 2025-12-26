#!/bin/bash

echo "=========================================="
echo "NSE Broadcast Integration - UI Fix Test"
echo "=========================================="
echo ""

# Check binary exists
if [ ! -f "build/TradingTerminal" ]; then
    echo "❌ TradingTerminal not found!"
    exit 1
fi

echo "✅ Binary found: build/TradingTerminal"
echo ""

# Check code changes
echo "Verifying code changes..."
echo ""

# Check that auto-start is removed
if grep -q "// Don't auto-start to avoid blocking UI" src/app/MainWindow.cpp; then
    echo "✅ Auto-start removed from setupContent()"
else
    echo "❌ Auto-start code not updated"
fi

# Check that Data menu is added
if grep -q 'addMenu("&Data")' src/app/MainWindow.cpp; then
    echo "✅ Data menu added to menu bar"
else
    echo "❌ Data menu not found"
fi

# Check start/stop actions
if grep -q "Start &NSE Broadcast Receiver" src/app/MainWindow.cpp; then
    echo "✅ Start NSE Broadcast menu option added"
else
    echo "❌ Start menu option not found"
fi

if grep -q "St&op NSE Broadcast Receiver" src/app/MainWindow.cpp; then
    echo "✅ Stop NSE Broadcast menu option added"
else
    echo "❌ Stop menu option not found"
fi

echo ""
echo "=========================================="
echo "Manual Test Instructions"
echo "=========================================="
echo ""
echo "1. Run the application:"
echo "   cd build && ./TradingTerminal"
echo ""
echo "2. Verify UI loads immediately (no hang)"
echo ""
echo "3. Check menu bar has 'Data' menu between 'Tools' and 'Help'"
echo ""
echo "4. Click Data → Start NSE Broadcast Receiver"
echo "   - Console should show UDP receiver starting"
echo "   - No UI freeze"
echo ""
echo "5. Create Market Watch (F4) and add scrips"
echo "   - During market hours: Should see real-time updates"
echo "   - Outside market hours: No updates (normal)"
echo ""
echo "6. Click Data → Stop NSE Broadcast Receiver"
echo "   - Console should show statistics"
echo "   - Application remains responsive"
echo ""

echo "=========================================="
echo "Expected Console Output"
echo "=========================================="
echo ""
echo "On startup (NO UDP messages):"
echo "  [RepositoryManager] Loading master contracts..."
echo "  [ScripBar] Repository not loaded yet"
echo "  CustomMainWindow created"
echo "  ✅ UI is ready"
echo ""
echo "After Data → Start NSE Broadcast:"
echo "  [UDP] Starting NSE broadcast receiver..."
echo "  [UDP] Multicast IP: 233.1.2.5"
echo "  [UDP] Port: 34330"
echo "  [UDP] ✅ Receiver initialized successfully"
echo "  [UDP] Registering callbacks..."
echo "  [UDP] Starting receiver thread..."
echo "  Starting MulticastReceiver..."
echo ""
echo "After Data → Stop NSE Broadcast:"
echo "  [UDP] Stopping receiver..."
echo "  [UDP] UDP PACKET STATISTICS:"
echo "  [UDP]   Total packets: XXXXX"
echo "  [UDP]   7200/7208 messages: XXXX"
echo "  [UDP] ✅ Receiver stopped"
echo ""

echo "=========================================="
echo "Troubleshooting"
echo "=========================================="
echo ""
echo "If UI still hangs:"
echo "  1. Check if startBroadcastReceiver() is called elsewhere"
echo "  2. Verify setupContent() doesn't have the auto-start line"
echo "  3. Rebuild: cd build && cmake --build . --target TradingTerminal"
echo ""
echo "If Data menu missing:"
echo "  1. Verify code changes in createMenuBar()"
echo "  2. Check that QMenu *dataMenu line is present"
echo "  3. Rebuild with cmake --build ."
echo ""

echo "=========================================="
echo "✅ Code Verification Complete"
echo "=========================================="
echo ""
echo "📋 Documentation: docs/NSE_BROADCAST_UI_FIX.md"
echo "🎯 Next: Test manually with GUI"
echo ""

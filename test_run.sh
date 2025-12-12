#!/bin/bash
# Test script to run Trading Terminal with debug output

cd "$(dirname "$0")/build"

echo "=== Trading Terminal Test Run ==="
echo "Testing menu bar positioning fix"
echo ""

./TradingTerminal.app/Contents/MacOS/TradingTerminal

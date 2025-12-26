#!/bin/bash
# Test script to run TradingTerminal and check for 7201 messages

echo "========================================"
echo "Starting TradingTerminal for 2 minutes"
echo "Checking for message 7201..."
echo "Press Ctrl+C to stop early"
echo "========================================"
echo ""

cd /home/ubuntu/Desktop/trading_terminal_cpp/build

# Run and capture output
./TradingTerminal 2>&1 | tee /tmp/udp_test.log &
PID=$!

echo "Process started with PID: $PID"
echo "Waiting 120 seconds..."

# Wait for 2 minutes
sleep 120

# Send SIGTERM (graceful shutdown)
echo ""
echo "Stopping application gracefully..."
kill -TERM $PID 2>/dev/null

# Wait for process to exit
sleep 2

# Force kill if still running
if ps -p $PID > /dev/null 2>&1; then
    echo "Force killing process..."
    kill -9 $PID 2>/dev/null
fi

echo ""
echo "========================================"
echo "SUMMARY"
echo "========================================"

# Extract message counts from log
echo ""
echo "Looking for UDP statistics in log..."
grep -A 20 "MESSAGE TYPE STATISTICS" /tmp/udp_test.log | tail -20

echo ""
echo "Looking for 7201 messages..."
MSGS_7201=$(grep -c "\[UDP 7201\]" /tmp/udp_test.log)
echo "Total 7201 message lines: $MSGS_7201"

echo ""
echo "Full log saved to: /tmp/udp_test.log"
echo "========================================"

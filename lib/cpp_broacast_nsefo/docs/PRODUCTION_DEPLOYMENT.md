# NSE UDP Reader - Production Deployment Guide

## Table of Contents
1. [System Requirements](#system-requirements)
2. [Installation](#installation)
3. [Configuration](#configuration)
4. [Security](#security)
5. [Monitoring](#monitoring)
6. [Operations](#operations)
7. [Troubleshooting](#troubleshooting)
8. [Disaster Recovery](#disaster-recovery)

---

## System Requirements

### Hardware
- **CPU**: 4+ cores (Intel Xeon or AMD EPYC recommended)
- **RAM**: 8GB minimum, 16GB recommended
- **Network**: 1Gbps NIC with multicast support
- **Storage**: 100GB for logs and data

### Software
- **OS**: Ubuntu 20.04 LTS or RHEL 8+
- **Compiler**: GCC 4.8+ with C++11 support
- **Libraries**: pthread, gtest (for testing)
- **Optional**: valgrind, perf (for profiling)

### Network
- Multicast-enabled network infrastructure
- Access to NSE multicast group (233.1.2.5:64555)
- Firewall rules allowing UDP multicast traffic

---

## Installation

### 1. Prepare Environment
```bash
# Update system
sudo apt-get update && sudo apt-get upgrade -y

# Install dependencies
sudo apt-get install -y build-essential g++ make git

# Create application user
sudo useradd -r -s /bin/false nse_reader
sudo mkdir -p /opt/nse_reader
sudo mkdir -p /var/log/nse_reader
sudo mkdir -p /etc/nse_reader
sudo chown -R nse_reader:nse_reader /opt/nse_reader /var/log/nse_reader
```

### 2. Build Application
```bash
# Clone/copy source
cd /opt/nse_reader
# ... copy source files ...

# Build
make clean && make

# Verify binary
./nse_reader --version || echo "Binary created successfully"
```

### 3. Install as Service
```bash
# Create systemd service
sudo tee /etc/systemd/system/nse_reader.service > /dev/null <<EOF
[Unit]
Description=NSE UDP Multicast Reader
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=nse_reader
Group=nse_reader
WorkingDirectory=/opt/nse_reader
ExecStart=/opt/nse_reader/nse_reader /etc/nse_reader/config.ini
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal
SyslogIdentifier=nse_reader

# Security hardening
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/log/nse_reader

[Install]
WantedBy=multi-user.target
EOF

# Reload systemd
sudo systemctl daemon-reload
```

---

## Configuration

### Production Configuration
```bash
# Create production config
sudo tee /etc/nse_reader/config.ini > /dev/null <<EOF
[Network]
multicast_ip = 233.1.2.5
port = 64555
buffer_size = 65535
socket_timeout_sec = 1

[Logging]
log_level = INFO
log_file = /var/log/nse_reader/app.log
log_to_console = false

[Statistics]
stats_interval_sec = 60
enable_stats = true

[Debug]
enable_hex_dump = false
hex_dump_size = 64
EOF

sudo chown nse_reader:nse_reader /etc/nse_reader/config.ini
sudo chmod 640 /etc/nse_reader/config.ini
```

### Log Rotation
```bash
# Create logrotate config
sudo tee /etc/logrotate.d/nse_reader > /dev/null <<EOF
/var/log/nse_reader/*.log {
    daily
    rotate 30
    compress
    delaycompress
    notifempty
    create 0640 nse_reader nse_reader
    sharedscripts
    postrotate
        systemctl reload nse_reader > /dev/null 2>&1 || true
    endscript
}
EOF
```

---

## Security

### Firewall Configuration
```bash
# Allow multicast traffic
sudo iptables -A INPUT -d 233.1.2.5 -p udp --dport 64555 -j ACCEPT

# Save rules
sudo netfilter-persistent save
```

### SELinux/AppArmor
```bash
# For SELinux (RHEL/CentOS)
sudo semanage fcontext -a -t bin_t "/opt/nse_reader/nse_reader"
sudo restorecon -v /opt/nse_reader/nse_reader

# For AppArmor (Ubuntu)
# Create profile if needed
```

### Network Security
- Use dedicated VLAN for market data
- Implement network segmentation
- Monitor for unauthorized multicast sources
- Enable network-level encryption if required

---

## Monitoring

### 1. Application Metrics

#### Prometheus Exporter (Future Enhancement)
```cpp
// Example metrics to expose:
// - nse_packets_received_total
// - nse_packets_processed_total
// - nse_decompression_failures_total
// - nse_processing_latency_microseconds
// - nse_messages_by_type
```

#### Current Monitoring
```bash
# Monitor via logs
tail -f /var/log/nse_reader/app.log

# Monitor via journalctl
journalctl -u nse_reader -f

# Check statistics output
grep "NSE MULTICAST" /var/log/nse_reader/app.log
```

### 2. System Metrics
```bash
# CPU and Memory
top -p $(pgrep nse_reader)

# Network traffic
sudo iftop -i eth0 -f "dst port 64555"

# Packet drops
netstat -su | grep -i drop
```

### 3. Alerting Rules

#### Nagios/Icinga
```bash
# Check if process is running
check_procs -c 1:1 -C nse_reader

# Check log for errors
check_log -F /var/log/nse_reader/app.log -q "ERROR"
```

#### Prometheus Alerts
```yaml
groups:
- name: nse_reader
  rules:
  - alert: NSEReaderDown
    expr: up{job="nse_reader"} == 0
    for: 1m
    annotations:
      summary: "NSE Reader is down"
  
  - alert: HighDecompressionFailures
    expr: rate(nse_decompression_failures_total[5m]) > 0.01
    for: 5m
    annotations:
      summary: "High decompression failure rate"
```

---

## Operations

### Starting the Service
```bash
# Enable on boot
sudo systemctl enable nse_reader

# Start service
sudo systemctl start nse_reader

# Check status
sudo systemctl status nse_reader
```

### Stopping the Service
```bash
# Stop gracefully
sudo systemctl stop nse_reader

# Force stop if needed
sudo systemctl kill -s SIGKILL nse_reader
```

### Restarting
```bash
# Restart service
sudo systemctl restart nse_reader

# Reload configuration (if supported)
sudo systemctl reload nse_reader
```

### Health Checks
```bash
#!/bin/bash
# healthcheck.sh

# Check if process is running
if ! pgrep -x nse_reader > /dev/null; then
    echo "ERROR: Process not running"
    exit 1
fi

# Check if receiving packets (last 60 seconds)
if ! grep -q "Total Packets" /var/log/nse_reader/app.log | tail -1; then
    echo "WARNING: No recent statistics"
    exit 1
fi

echo "OK: Service healthy"
exit 0
```

---

## Troubleshooting

### Common Issues

#### 1. No Packets Received
**Symptoms**: Zero packets in statistics

**Diagnosis**:
```bash
# Check multicast route
ip route show | grep 224.0.0.0

# Verify multicast membership
netstat -g | grep 233.1.2.5

# Capture packets
sudo tcpdump -i any -n 'dst host 233.1.2.5 and dst port 64555'
```

**Solution**:
```bash
# Add multicast route
sudo route add -net 224.0.0.0 netmask 240.0.0.0 dev eth0

# Check firewall
sudo iptables -L -n | grep 233.1.2.5
```

#### 2. High CPU Usage
**Diagnosis**:
```bash
# Profile application
sudo perf top -p $(pgrep nse_reader)

# Check for tight loops
sudo strace -p $(pgrep nse_reader) -c
```

**Solution**:
- Review recent code changes
- Check for infinite loops
- Verify packet rate is within expected range

#### 3. Memory Leaks
**Diagnosis**:
```bash
# Monitor memory over time
while true; do
    ps aux | grep nse_reader | grep -v grep
    sleep 60
done

# Run valgrind
sudo -u nse_reader valgrind --leak-check=full ./nse_reader
```

**Solution**:
- Review recent allocations
- Check for missing `delete`/`free` calls
- Verify RAII patterns

#### 4. Decompression Failures
**Symptoms**: High failure count in statistics

**Diagnosis**:
```bash
# Check error logs
grep "Decompression failed" /var/log/nse_reader/app.log

# Enable debug logging
NSE_LOG_LEVEL=DEBUG ./nse_reader
```

**Solution**:
- Verify packet integrity
- Check for network corruption
- Update LZO library if needed

---

## Disaster Recovery

### Backup Strategy
```bash
# Backup configuration
sudo tar -czf /backup/nse_reader_config_$(date +%Y%m%d).tar.gz \
    /etc/nse_reader/

# Backup logs (if needed)
sudo tar -czf /backup/nse_reader_logs_$(date +%Y%m%d).tar.gz \
    /var/log/nse_reader/
```

### Recovery Procedures

#### 1. Service Crash
```bash
# Check crash logs
journalctl -u nse_reader --since "1 hour ago"

# Restart service
sudo systemctl restart nse_reader

# If persistent, restore from backup
sudo systemctl stop nse_reader
sudo tar -xzf /backup/nse_reader_config_latest.tar.gz -C /
sudo systemctl start nse_reader
```

#### 2. Data Loss
```bash
# Restore configuration
sudo tar -xzf /backup/nse_reader_config_YYYYMMDD.tar.gz -C /

# Verify configuration
sudo -u nse_reader /opt/nse_reader/nse_reader --check-config

# Restart service
sudo systemctl restart nse_reader
```

#### 3. Complete System Failure
```bash
# On new system:
# 1. Install OS and dependencies
# 2. Restore application
sudo tar -xzf /backup/nse_reader_full_YYYYMMDD.tar.gz -C /opt/

# 3. Restore configuration
sudo tar -xzf /backup/nse_reader_config_YYYYMMDD.tar.gz -C /

# 4. Recreate systemd service
# (see Installation section)

# 5. Start service
sudo systemctl start nse_reader
```

---

## Performance Tuning

### Network Tuning
```bash
# Increase UDP buffer sizes
sudo sysctl -w net.core.rmem_max=134217728
sudo sysctl -w net.core.rmem_default=134217728

# Make permanent
echo "net.core.rmem_max=134217728" | sudo tee -a /etc/sysctl.conf
echo "net.core.rmem_default=134217728" | sudo tee -a /etc/sysctl.conf
```

### CPU Affinity
```bash
# Pin to specific CPUs
sudo systemctl set-property nse_reader.service CPUAffinity=0,1,2,3
```

### Priority
```bash
# Increase process priority
sudo systemctl set-property nse_reader.service Nice=-10
```

---

## Compliance and Auditing

### Audit Logging
```bash
# Enable audit for configuration changes
sudo auditctl -w /etc/nse_reader/ -p wa -k nse_config_changes

# View audit logs
sudo ausearch -k nse_config_changes
```

### Access Control
```bash
# Restrict access to configuration
sudo chmod 640 /etc/nse_reader/config.ini
sudo chown root:nse_reader /etc/nse_reader/config.ini

# Restrict access to logs
sudo chmod 640 /var/log/nse_reader/*.log
```

---

## Appendix

### A. Useful Commands
```bash
# View real-time statistics
watch -n 5 'journalctl -u nse_reader -n 50 | grep "Total Packets"'

# Monitor packet rate
sudo iftop -i eth0 -f "dst port 64555"

# Check service dependencies
systemctl list-dependencies nse_reader

# View service logs with timestamps
journalctl -u nse_reader --since today --no-pager
```

### B. Configuration Examples

#### High-Performance Config
```ini
[Network]
buffer_size = 131072  # 128KB buffer
socket_timeout_sec = 0  # No timeout (blocking)

[Logging]
log_level = WARN  # Minimal logging
log_to_console = false

[Statistics]
stats_interval_sec = 300  # Less frequent stats
```

#### Debug Config
```ini
[Logging]
log_level = DEBUG
log_to_console = true

[Debug]
enable_hex_dump = true
hex_dump_size = 128
```

---

## Support

### Contact Information
- **Technical Support**: [your-support-email]
- **Emergency Hotline**: [emergency-number]
- **Documentation**: [wiki-url]

### Escalation Path
1. Level 1: Application logs and basic troubleshooting
2. Level 2: System administrator review
3. Level 3: Development team involvement
4. Level 4: Vendor support (NSE)

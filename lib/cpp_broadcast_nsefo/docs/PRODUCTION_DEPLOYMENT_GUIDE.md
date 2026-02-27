# NSE UDP Reader - Production Deployment Guide

## Table of Contents

1. [System Requirements](#system-requirements)
2. [Pre-Deployment Checklist](#pre-deployment-checklist)
3. [Installation](#installation)
4. [Configuration](#configuration)
5. [Security Hardening](#security-hardening)
6. [Monitoring Setup](#monitoring-setup)
7. [High Availability](#high-availability)
8. [Backup and Recovery](#backup-and-recovery)
9. [Operational Runbooks](#operational-runbooks)
10. [Troubleshooting](#troubleshooting)

---

## System Requirements

### Hardware Requirements

#### Minimum (Development/Testing)
- **CPU:** 4 cores @ 2.4 GHz
- **RAM:** 8 GB
- **Network:** 1 Gbps NIC
- **Storage:** 50 GB SSD
- **OS:** Ubuntu 20.04 LTS or later

#### Recommended (Production)
- **CPU:** 8 cores @ 3.0 GHz (Intel Xeon or AMD EPYC)
- **RAM:** 16 GB DDR4
- **Network:** 10 Gbps NIC with multicast support
- **Storage:** 100 GB NVMe SSD
- **OS:** Ubuntu 22.04 LTS

#### High-Performance (100K+ pps)
- **CPU:** 16 cores @ 3.5 GHz with AVX2 support
- **RAM:** 32 GB DDR4-3200
- **Network:** 25 Gbps NIC with SR-IOV
- **Storage:** 200 GB NVMe SSD (RAID 1)
- **OS:** Ubuntu 22.04 LTS with real-time kernel

### Software Requirements

```bash
# Required packages
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    libgtest-dev \
    liblzo2-dev \
    net-tools \
    ethtool \
    sysstat \
    htop \
    iotop \
    tcpdump \
    wireshark-common
```

### Network Requirements

- **Multicast Support:** Network must support IGMP
- **Firewall:** Allow UDP port 64555 (or configured port)
- **Bandwidth:** Minimum 100 Mbps, recommended 1 Gbps
- **Latency:** < 1ms to NSE multicast source
- **Packet Loss:** < 0.01%

---

## Pre-Deployment Checklist

### Infrastructure
- [ ] Server provisioned with required specifications
- [ ] Network connectivity verified
- [ ] Multicast routing configured
- [ ] Firewall rules configured
- [ ] NTP synchronized
- [ ] DNS configured
- [ ] Backup storage available

### Software
- [ ] Operating system updated
- [ ] Required packages installed
- [ ] Application compiled and tested
- [ ] Configuration files prepared
- [ ] Monitoring tools installed
- [ ] Log rotation configured

### Security
- [ ] User accounts created with minimal privileges
- [ ] SSH keys configured
- [ ] Firewall rules applied
- [ ] SELinux/AppArmor configured
- [ ] Audit logging enabled
- [ ] Security patches applied

### Documentation
- [ ] Network diagram available
- [ ] Configuration documented
- [ ] Runbooks prepared
- [ ] Contact list updated
- [ ] Escalation procedures defined

---

## Installation

### Step 1: Create Deployment User

```bash
# Create dedicated user
sudo useradd -r -m -s /bin/bash nsereader
sudo usermod -aG sudo nsereader  # Optional: for admin tasks

# Create directory structure
sudo mkdir -p /opt/nsereader/{bin,config,logs,data}
sudo chown -R nsereader:nsereader /opt/nsereader
```

### Step 2: Clone and Build

```bash
# Switch to deployment user
sudo su - nsereader

# Clone repository
cd /opt/nsereader
git clone https://github.com/your-org/nse-udp-reader.git src
cd src

# Checkout specific version (recommended for production)
git checkout v1.0.0

# Build with optimizations
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/opt/nsereader \
      -DENABLE_TESTING=OFF \
      ..
make -j$(nproc)
make install
```

### Step 3: Verify Installation

```bash
# Check binary
/opt/nsereader/bin/nse_reader --version

# Run self-test
/opt/nsereader/bin/nse_reader --test

# Check dependencies
ldd /opt/nsereader/bin/nse_reader
```

---

## Configuration

### Main Configuration File

**File:** `/opt/nsereader/config/config.ini`

```ini
[network]
# Multicast group and port
multicast_ip = 233.1.2.5
port = 64555

# Network interface (leave empty for default)
interface = eth0

# Socket buffer size (bytes)
socket_buffer_size = 8388608  # 8 MB

[processing]
# Number of worker threads (0 = auto-detect)
worker_threads = 4

# Queue size for packet processing
queue_size = 10000

# Enable/disable compression
enable_compression = true

[logging]
# Log level: DEBUG, INFO, WARN, ERROR
log_level = INFO

# Log file path
log_file = /opt/nsereader/logs/nsereader.log

# Log rotation
max_log_size_mb = 100
max_log_files = 10

# Enable console logging
console_logging = false

[monitoring]
# Enable metrics export
enable_metrics = true

# HTTP server port for metrics
metrics_port = 9090

# Health check interval (seconds)
health_check_interval = 30

[performance]
# CPU affinity (comma-separated core IDs)
cpu_affinity = 0,1,2,3

# Enable NUMA awareness
numa_aware = true

# Packet processing timeout (ms)
processing_timeout_ms = 1000

[database]
# Enable database storage (if applicable)
enable_database = false

# Database connection string
# db_connection = postgresql://user:pass@localhost/nsedata
```

### Environment Variables

```bash
# /opt/nsereader/config/environment
export NSE_CONFIG_FILE=/opt/nsereader/config/config.ini
export NSE_LOG_DIR=/opt/nsereader/logs
export NSE_DATA_DIR=/opt/nsereader/data
export NSE_METRICS_PORT=9090
```

### Systemd Service

**File:** `/etc/systemd/system/nsereader.service`

```ini
[Unit]
Description=NSE UDP Reader Service
After=network.target
Wants=network-online.target

[Service]
Type=simple
User=nsereader
Group=nsereader
WorkingDirectory=/opt/nsereader

# Environment
EnvironmentFile=/opt/nsereader/config/environment

# Execution
ExecStart=/opt/nsereader/bin/nse_reader --config ${NSE_CONFIG_FILE}
ExecReload=/bin/kill -HUP $MAINPID

# Restart policy
Restart=always
RestartSec=10s

# Resource limits
LimitNOFILE=65536
LimitNPROC=4096
MemoryLimit=2G
CPUQuota=400%  # 4 cores

# Security
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/opt/nsereader/logs /opt/nsereader/data

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=nsereader

[Install]
WantedBy=multi-user.target
```

### Enable and Start Service

```bash
# Reload systemd
sudo systemctl daemon-reload

# Enable service to start on boot
sudo systemctl enable nsereader

# Start service
sudo systemctl start nsereader

# Check status
sudo systemctl status nsereader

# View logs
sudo journalctl -u nsereader -f
```

---

## Security Hardening

### 1. Firewall Configuration

```bash
# Allow incoming UDP multicast
sudo ufw allow in on eth0 to 233.1.2.5 port 64555 proto udp

# Allow metrics endpoint (from monitoring server only)
sudo ufw allow from 10.0.0.100 to any port 9090 proto tcp

# Enable firewall
sudo ufw enable
```

### 2. Network Security

```bash
# Disable IP forwarding (if not needed)
sudo sysctl -w net.ipv4.ip_forward=0

# Enable SYN cookies
sudo sysctl -w net.ipv4.tcp_syncookies=1

# Disable ICMP redirects
sudo sysctl -w net.ipv4.conf.all.accept_redirects=0
sudo sysctl -w net.ipv4.conf.all.send_redirects=0

# Make permanent
echo "net.ipv4.ip_forward=0" | sudo tee -a /etc/sysctl.conf
echo "net.ipv4.tcp_syncookies=1" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
```

### 3. Application Security

```bash
# Set file permissions
sudo chmod 750 /opt/nsereader/bin/nse_reader
sudo chmod 640 /opt/nsereader/config/config.ini
sudo chmod 750 /opt/nsereader/logs
sudo chmod 750 /opt/nsereader/data

# Set ownership
sudo chown -R nsereader:nsereader /opt/nsereader
```

### 4. SELinux Configuration (RHEL/CentOS)

```bash
# Create SELinux policy (if needed)
sudo semanage port -a -t nsereader_port_t -p udp 64555
sudo semanage port -a -t nsereader_port_t -p tcp 9090

# Set context
sudo semanage fcontext -a -t nsereader_exec_t "/opt/nsereader/bin/nse_reader"
sudo restorecon -v /opt/nsereader/bin/nse_reader
```

### 5. Audit Logging

```bash
# Add audit rules
sudo auditctl -w /opt/nsereader/bin/nse_reader -p x -k nsereader_exec
sudo auditctl -w /opt/nsereader/config/ -p wa -k nsereader_config

# Make permanent
echo "-w /opt/nsereader/bin/nse_reader -p x -k nsereader_exec" | \
    sudo tee -a /etc/audit/rules.d/nsereader.rules
```

---

## Monitoring Setup

### 1. Prometheus Configuration

**File:** `/etc/prometheus/prometheus.yml`

```yaml
scrape_configs:
  - job_name: 'nsereader'
    scrape_interval: 15s
    static_configs:
      - targets: ['localhost:9090']
        labels:
          environment: 'production'
          datacenter: 'dc1'
```

### 2. Node Exporter (System Metrics)

```bash
# Install node_exporter
wget https://github.com/prometheus/node_exporter/releases/download/v1.6.1/node_exporter-1.6.1.linux-amd64.tar.gz
tar xvfz node_exporter-*.tar.gz
sudo cp node_exporter-*/node_exporter /usr/local/bin/
sudo chown nsereader:nsereader /usr/local/bin/node_exporter

# Create systemd service
sudo tee /etc/systemd/system/node_exporter.service << EOF
[Unit]
Description=Node Exporter
After=network.target

[Service]
User=nsereader
Group=nsereader
Type=simple
ExecStart=/usr/local/bin/node_exporter

[Install]
WantedBy=multi-user.target
EOF

# Start service
sudo systemctl daemon-reload
sudo systemctl enable node_exporter
sudo systemctl start node_exporter
```

### 3. Grafana Dashboard

```bash
# Import dashboard
# Dashboard ID: (to be created)
# Or use JSON file: monitoring/grafana_dashboard.json
```

---

## High Availability

### Active-Passive Setup

```
┌─────────────┐
│   Primary   │ ◄─── Active
│  (Server 1) │
└──────┬──────┘
       │
       │ Heartbeat
       │
┌──────▼──────┐
│  Secondary  │ ◄─── Standby
│  (Server 2) │
└─────────────┘
```

#### Keepalived Configuration

**File:** `/etc/keepalived/keepalived.conf` (Primary)

```conf
vrrp_instance NSE_READER {
    state MASTER
    interface eth0
    virtual_router_id 51
    priority 100
    advert_int 1
    
    authentication {
        auth_type PASS
        auth_pass secret123
    }
    
    virtual_ipaddress {
        10.0.0.50/24
    }
    
    track_script {
        check_nsereader
    }
}

vrrp_script check_nsereader {
    script "/usr/local/bin/check_nsereader.sh"
    interval 5
    weight -20
}
```

**File:** `/usr/local/bin/check_nsereader.sh`

```bash
#!/bin/bash
systemctl is-active --quiet nsereader
exit $?
```

### Load Balancing (Multiple Instances)

For horizontal scaling, use multiple instances with different multicast groups or implement message partitioning.

---

## Backup and Recovery

### 1. Configuration Backup

```bash
#!/bin/bash
# /opt/nsereader/scripts/backup_config.sh

BACKUP_DIR=/opt/nsereader/backups
DATE=$(date +%Y%m%d_%H%M%S)

mkdir -p $BACKUP_DIR

# Backup configuration
tar -czf $BACKUP_DIR/config_$DATE.tar.gz \
    /opt/nsereader/config/

# Keep only last 30 days
find $BACKUP_DIR -name "config_*.tar.gz" -mtime +30 -delete
```

### 2. Log Archival

```bash
#!/bin/bash
# /opt/nsereader/scripts/archive_logs.sh

LOG_DIR=/opt/nsereader/logs
ARCHIVE_DIR=/opt/nsereader/archives
DATE=$(date +%Y%m%d)

mkdir -p $ARCHIVE_DIR

# Compress and archive old logs
find $LOG_DIR -name "*.log.*" -mtime +7 -exec \
    tar -czf $ARCHIVE_DIR/logs_$DATE.tar.gz {} + -delete

# Upload to S3 (optional)
# aws s3 cp $ARCHIVE_DIR/logs_$DATE.tar.gz s3://bucket/nsereader/logs/
```

### 3. Automated Backups (Cron)

```bash
# Add to crontab
sudo crontab -e -u nsereader

# Backup config daily at 2 AM
0 2 * * * /opt/nsereader/scripts/backup_config.sh

# Archive logs weekly on Sunday at 3 AM
0 3 * * 0 /opt/nsereader/scripts/archive_logs.sh
```

### 4. Disaster Recovery

```bash
# Restore from backup
tar -xzf /opt/nsereader/backups/config_20250117_020000.tar.gz -C /

# Restart service
sudo systemctl restart nsereader
```

---

## Operational Runbooks

### Runbook 1: Service Not Starting

**Symptoms:** Service fails to start, exits immediately

**Diagnosis:**
```bash
# Check service status
sudo systemctl status nsereader

# Check logs
sudo journalctl -u nsereader -n 100 --no-pager

# Check configuration
/opt/nsereader/bin/nse_reader --config /opt/nsereader/config/config.ini --validate

# Check network
ip maddr show
netstat -g
```

**Resolution:**
1. Verify configuration file syntax
2. Check network interface exists
3. Verify multicast group is reachable
4. Check file permissions
5. Review error logs for specific issues

### Runbook 2: High Packet Loss

**Symptoms:** `nse_packets_dropped_total` increasing rapidly

**Diagnosis:**
```bash
# Check system resources
top
free -h
iostat -x 1

# Check network statistics
netstat -su | grep -i drop
ethtool -S eth0 | grep -i drop

# Check application metrics
curl http://localhost:9090/metrics | grep dropped
```

**Resolution:**
1. Increase socket buffer size in config
2. Add more worker threads
3. Check CPU usage and add cores if needed
4. Verify network bandwidth
5. Check for disk I/O bottlenecks

### Runbook 3: Memory Leak

**Symptoms:** Memory usage continuously increasing

**Diagnosis:**
```bash
# Monitor memory over time
watch -n 5 'ps aux | grep nse_reader'

# Check for memory leaks with valgrind (in test environment)
valgrind --leak-check=full --show-leak-kinds=all \
    /opt/nsereader/bin/nse_reader --config config.ini
```

**Resolution:**
1. Restart service as immediate mitigation
2. Review recent code changes
3. Run valgrind analysis
4. Check for unbounded data structures
5. Apply memory limit in systemd service

### Runbook 4: Network Connectivity Issues

**Symptoms:** No packets received, network errors

**Diagnosis:**
```bash
# Check interface status
ip link show eth0

# Check multicast membership
ip maddr show eth0

# Capture multicast traffic
sudo tcpdump -i eth0 -n host 233.1.2.5 and udp port 64555

# Check routing
ip route show
```

**Resolution:**
1. Verify network cable connected
2. Check switch multicast configuration
3. Verify IGMP snooping settings
4. Check firewall rules
5. Restart network interface if needed

### Runbook 5: High CPU Usage

**Symptoms:** CPU usage > 80%, high latency

**Diagnosis:**
```bash
# Check CPU usage by thread
top -H -p $(pgrep nse_reader)

# Profile with perf
sudo perf record -p $(pgrep nse_reader) -g -- sleep 30
sudo perf report

# Check CPU frequency scaling
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
```

**Resolution:**
1. Set CPU governor to 'performance'
2. Optimize worker thread count
3. Enable CPU affinity
4. Check for inefficient code paths
5. Consider hardware upgrade

---

## Troubleshooting

### Common Issues

#### Issue: "Permission denied" on multicast join

**Solution:**
```bash
# Add capability to binary
sudo setcap cap_net_raw+ep /opt/nsereader/bin/nse_reader

# Or run as root (not recommended)
```

#### Issue: "Address already in use"

**Solution:**
```bash
# Find process using port
sudo lsof -i :64555

# Kill old process
sudo kill -9 <PID>

# Or change port in configuration
```

#### Issue: High decompression failures

**Solution:**
```bash
# Check LZO library version
ldd /opt/nsereader/bin/nse_reader | grep lzo

# Verify compressed data integrity
# Review decompression error logs
```

### Debug Mode

```bash
# Run in foreground with debug logging
/opt/nsereader/bin/nse_reader \
    --config /opt/nsereader/config/config.ini \
    --log-level DEBUG \
    --console

# Enable core dumps
ulimit -c unlimited
echo "/tmp/core.%e.%p" | sudo tee /proc/sys/kernel/core_pattern
```

### Performance Tuning

```bash
# Increase socket buffer
sudo sysctl -w net.core.rmem_max=134217728
sudo sysctl -w net.core.rmem_default=134217728

# Increase UDP buffer
sudo sysctl -w net.ipv4.udp_mem="102400 873800 16777216"

# Disable CPU frequency scaling
sudo cpupower frequency-set -g performance

# Set CPU affinity
taskset -c 0-3 /opt/nsereader/bin/nse_reader
```

---

## Maintenance

### Regular Tasks

#### Daily
- [ ] Check service status
- [ ] Review error logs
- [ ] Monitor packet drop rate
- [ ] Verify disk space

#### Weekly
- [ ] Review performance metrics
- [ ] Check for security updates
- [ ] Archive old logs
- [ ] Test backup restoration

#### Monthly
- [ ] Review capacity planning
- [ ] Update documentation
- [ ] Conduct DR drill
- [ ] Review and update runbooks

### Upgrade Procedure

```bash
# 1. Backup current version
sudo systemctl stop nsereader
cp -r /opt/nsereader /opt/nsereader.backup

# 2. Deploy new version
cd /opt/nsereader/src
git fetch
git checkout v1.1.0
cd build
make clean
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
make install

# 3. Test new version
/opt/nsereader/bin/nse_reader --version
/opt/nsereader/bin/nse_reader --test

# 4. Start service
sudo systemctl start nsereader

# 5. Monitor for issues
sudo journalctl -u nsereader -f

# 6. Rollback if needed
# sudo systemctl stop nsereader
# rm -rf /opt/nsereader
# mv /opt/nsereader.backup /opt/nsereader
# sudo systemctl start nsereader
```

---

## Appendix

### A. Network Diagram

```
                    ┌──────────────┐
                    │  NSE Exchange│
                    │   Multicast  │
                    │  233.1.2.5   │
                    └──────┬───────┘
                           │
                    ┌──────▼───────┐
                    │   Network    │
                    │    Switch    │
                    └──────┬───────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
        ┌─────▼─────┐ ┌───▼────┐ ┌────▼─────┐
        │ NSE Reader│ │Monitoring│ │  Backup  │
        │  Primary  │ │  Server  │ │  Server  │
        └───────────┘ └──────────┘ └──────────┘
```

### B. Contact Information

| Role | Name | Email | Phone | Escalation |
|------|------|-------|-------|------------|
| Primary On-Call | TBD | oncall@example.com | +1-xxx-xxx-xxxx | L1 |
| Team Lead | TBD | lead@example.com | +1-xxx-xxx-xxxx | L2 |
| Manager | TBD | manager@example.com | +1-xxx-xxx-xxxx | L3 |

### C. SLA Targets

| Metric | Target | Measurement |
|--------|--------|-------------|
| Uptime | 99.9% | Monthly |
| Packet Loss | < 0.01% | Per hour |
| Processing Latency (P99) | < 100μs | Per minute |
| Recovery Time (RTO) | < 5 minutes | Per incident |
| Recovery Point (RPO) | < 1 minute | Per incident |

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-12-17 | AI Assistant | Initial version |

---

**End of Document**

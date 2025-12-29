# NSE UDP Reader - Monitoring and Alerting Implementation Plan

## Overview

This document outlines the comprehensive monitoring and alerting strategy for the NSE UDP Reader application.

**Timeline:** 3 weeks  
**Priority:** High  
**Dependencies:** UDPStats system, Configuration system, Logging framework

---

## Objectives

1. **Real-time visibility** into application health and performance
2. **Proactive alerting** on anomalies and issues
3. **Integration** with industry-standard monitoring tools
4. **Production-ready** observability stack

---

## Architecture

```
┌─────────────────┐
│  NSE UDP Reader │
│                 │
│  ┌───────────┐  │
│  │ UDPStats  │  │
│  └─────┬─────┘  │
│        │        │
│  ┌─────▼─────┐  │
│  │ Metrics   │  │
│  │ Exporter  │  │
│  └─────┬─────┘  │
└────────┼────────┘
         │
         │ HTTP :9090
         │
    ┌────▼────┐
    │Prometheus│
    └────┬────┘
         │
    ┌────▼────┐
    │ Grafana │
    └─────────┘
         │
    ┌────▼────┐
    │AlertMgr │
    └─────────┘
```

---

## Phase 1: Metrics Collection (Week 1)

### 1.1 Core Metrics

#### Packet Metrics
```cpp
// Counters
- nse_packets_received_total
- nse_packets_processed_total
- nse_packets_dropped_total
- nse_packets_malformed_total
- nse_bytes_received_total
- nse_bytes_processed_total

// By transaction code
- nse_messages_by_type{tx_code="7200"}
- nse_compression_ratio{tx_code="7200"}
```

#### Performance Metrics
```cpp
// Histograms (microseconds)
- nse_packet_processing_duration_us
- nse_decompression_duration_us
- nse_parsing_duration_us

// Gauges
- nse_processing_lag_ms
- nse_queue_depth
- nse_buffer_utilization_percent
```

#### Error Metrics
```cpp
// Counters
- nse_decompression_failures_total
- nse_parsing_errors_total
- nse_network_errors_total
- nse_sequence_gaps_total
```

#### System Metrics
```cpp
// Gauges
- nse_cpu_usage_percent
- nse_memory_usage_bytes
- nse_thread_count
- nse_uptime_seconds
```

### 1.2 Metrics Implementation

#### File: `include/metrics.h`
```cpp
#ifndef METRICS_H
#define METRICS_H

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>

class Metrics {
public:
    static Metrics& getInstance();
    
    // Counter operations
    void incrementCounter(const std::string& name, uint64_t value = 1);
    void incrementCounterWithLabel(const std::string& name, 
                                   const std::string& label, 
                                   const std::string& labelValue,
                                   uint64_t value = 1);
    
    // Gauge operations
    void setGauge(const std::string& name, double value);
    void incrementGauge(const std::string& name, double value);
    void decrementGauge(const std::string& name, double value);
    
    // Histogram operations
    void observeHistogram(const std::string& name, double value);
    
    // Export metrics in Prometheus format
    std::string exportPrometheus();
    
    // Reset all metrics
    void reset();
    
private:
    Metrics() = default;
    ~Metrics() = default;
    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;
    
    struct Counter {
        std::atomic<uint64_t> value{0};
    };
    
    struct Gauge {
        std::atomic<double> value{0.0};
    };
    
    struct Histogram {
        std::vector<double> observations;
        std::mutex mutex;
        
        struct Quantiles {
            double p50, p90, p95, p99, p999;
            double sum, count, min, max;
        };
        
        Quantiles calculate() const;
    };
    
    std::map<std::string, Counter> counters_;
    std::map<std::string, Gauge> gauges_;
    std::map<std::string, Histogram> histograms_;
    std::mutex mutex_;
};

// RAII timer for automatic duration measurement
class ScopedTimer {
public:
    ScopedTimer(const std::string& metricName)
        : metricName_(metricName),
          start_(std::chrono::high_resolution_clock::now()) {}
    
    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start_).count();
        Metrics::getInstance().observeHistogram(metricName_, duration);
    }
    
private:
    std::string metricName_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

#endif // METRICS_H
```

---

## Phase 2: HTTP Metrics Endpoint (Week 1)

### 2.1 Simple HTTP Server

#### File: `include/http_server.h`
```cpp
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <atomic>
#include <string>
#include <thread>

class HttpServer {
public:
    HttpServer(int port);
    ~HttpServer();
    
    void start();
    void stop();
    
    bool isRunning() const { return running_; }
    
private:
    void serverLoop();
    void handleRequest(int clientSocket);
    std::string handleMetricsRequest();
    std::string handleHealthRequest();
    std::string handleReadyRequest();
    
    int port_;
    int serverSocket_;
    std::atomic<bool> running_{false};
    std::thread serverThread_;
};

#endif // HTTP_SERVER_H
```

### 2.2 Endpoints

| Endpoint | Method | Description | Response |
|----------|--------|-------------|----------|
| `/metrics` | GET | Prometheus metrics | text/plain |
| `/health` | GET | Health check | JSON |
| `/ready` | GET | Readiness probe | JSON |
| `/stats` | GET | Human-readable stats | JSON |

---

## Phase 3: Health Checks (Week 2)

### 3.1 Health Check System

#### File: `include/health_check.h`
```cpp
#ifndef HEALTH_CHECK_H
#define HEALTH_CHECK_H

#include <chrono>
#include <string>

enum class HealthStatus {
    HEALTHY,
    DEGRADED,
    UNHEALTHY
};

struct HealthCheckResult {
    HealthStatus status;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
};

class HealthCheck {
public:
    static HealthCheck& getInstance();
    
    // Component health checks
    HealthCheckResult checkReceiver();
    HealthCheckResult checkProcessing();
    HealthCheckResult checkQueue();
    HealthCheckResult checkMemory();
    HealthCheckResult checkNetwork();
    
    // Overall health
    HealthCheckResult checkOverall();
    
    // Update last activity timestamps
    void updateLastPacketReceived();
    void updateLastPacketProcessed();
    
    // Export as JSON
    std::string toJson();
    
private:
    HealthCheck() = default;
    
    std::chrono::system_clock::time_point lastPacketReceived_;
    std::chrono::system_clock::time_point lastPacketProcessed_;
    std::chrono::system_clock::time_point startTime_;
};

#endif // HEALTH_CHECK_H
```

### 3.2 Health Check Criteria

| Component | Healthy | Degraded | Unhealthy |
|-----------|---------|----------|-----------|
| **Receiver** | Packets received < 5s ago | 5-30s | > 30s |
| **Processing** | Lag < 100ms | 100ms-1s | > 1s |
| **Queue** | Depth < 50% | 50-90% | > 90% |
| **Memory** | < 200MB | 200-400MB | > 400MB |
| **Network** | Error rate < 0.1% | 0.1-1% | > 1% |

---

## Phase 4: Prometheus Integration (Week 2)

### 4.1 Prometheus Configuration

#### File: `monitoring/prometheus.yml`
```yaml
global:
  scrape_interval: 15s
  evaluation_interval: 15s
  external_labels:
    cluster: 'nse-production'
    environment: 'prod'

scrape_configs:
  - job_name: 'nse_reader'
    static_configs:
      - targets: ['localhost:9090']
        labels:
          instance: 'nse-reader-01'
          datacenter: 'dc1'
    
    metric_relabel_configs:
      # Drop high-cardinality metrics if needed
      - source_labels: [__name__]
        regex: 'nse_.*_bucket'
        action: drop

  - job_name: 'node_exporter'
    static_configs:
      - targets: ['localhost:9100']
```

### 4.2 Recording Rules

#### File: `monitoring/recording_rules.yml`
```yaml
groups:
  - name: nse_reader_aggregations
    interval: 30s
    rules:
      # Packet rate (packets/sec)
      - record: nse:packet_rate:1m
        expr: rate(nse_packets_received_total[1m])
      
      - record: nse:packet_rate:5m
        expr: rate(nse_packets_received_total[5m])
      
      # Processing rate
      - record: nse:processing_rate:1m
        expr: rate(nse_packets_processed_total[1m])
      
      # Drop rate
      - record: nse:drop_rate:1m
        expr: rate(nse_packets_dropped_total[1m])
      
      # Error rate
      - record: nse:error_rate:1m
        expr: (
          rate(nse_decompression_failures_total[1m]) +
          rate(nse_parsing_errors_total[1m]) +
          rate(nse_network_errors_total[1m])
        )
      
      # Latency percentiles
      - record: nse:processing_latency_p99:1m
        expr: histogram_quantile(0.99, rate(nse_packet_processing_duration_us_bucket[1m]))
      
      - record: nse:processing_latency_p95:1m
        expr: histogram_quantile(0.95, rate(nse_packet_processing_duration_us_bucket[1m]))
```

---

## Phase 5: Alerting Rules (Week 3)

### 5.1 Alert Definitions

#### File: `monitoring/alert_rules.yml`
```yaml
groups:
  - name: nse_reader_critical
    rules:
      # Application down
      - alert: NSEReaderDown
        expr: up{job="nse_reader"} == 0
        for: 1m
        labels:
          severity: critical
          component: application
        annotations:
          summary: "NSE Reader is down"
          description: "NSE Reader on {{ $labels.instance }} has been down for more than 1 minute"
          runbook: "https://wiki.example.com/runbooks/nse-reader-down"
      
      # No packets received
      - alert: NSENoPacketsReceived
        expr: rate(nse_packets_received_total[5m]) == 0
        for: 2m
        labels:
          severity: critical
          component: receiver
        annotations:
          summary: "No packets received"
          description: "NSE Reader has not received any packets in the last 5 minutes"
          runbook: "https://wiki.example.com/runbooks/no-packets"
      
      # High packet drop rate
      - alert: NSEHighPacketDropRate
        expr: rate(nse_packets_dropped_total[5m]) > 100
        for: 5m
        labels:
          severity: critical
          component: processing
        annotations:
          summary: "High packet drop rate"
          description: "Dropping {{ $value | humanize }} packets/sec on {{ $labels.instance }}"
          runbook: "https://wiki.example.com/runbooks/packet-drops"
  
  - name: nse_reader_warning
    rules:
      # High processing latency
      - alert: NSEHighProcessingLatency
        expr: nse:processing_latency_p99:1m > 1000
        for: 5m
        labels:
          severity: warning
          component: performance
        annotations:
          summary: "High processing latency"
          description: "P99 latency is {{ $value | humanize }}μs (threshold: 1000μs)"
      
      # High decompression failures
      - alert: NSEHighDecompressionFailures
        expr: rate(nse_decompression_failures_total[5m]) > 10
        for: 5m
        labels:
          severity: warning
          component: decompression
        annotations:
          summary: "High decompression failure rate"
          description: "{{ $value | humanize }} decompression failures/sec"
      
      # High memory usage
      - alert: NSEHighMemoryUsage
        expr: nse_memory_usage_bytes > 400000000  # 400MB
        for: 10m
        labels:
          severity: warning
          component: system
        annotations:
          summary: "High memory usage"
          description: "Memory usage is {{ $value | humanizeBytes }} (threshold: 400MB)"
      
      # Sequence gaps detected
      - alert: NSESequenceGaps
        expr: rate(nse_sequence_gaps_total[5m]) > 1
        for: 5m
        labels:
          severity: warning
          component: receiver
        annotations:
          summary: "Packet sequence gaps detected"
          description: "{{ $value | humanize }} sequence gaps/sec - possible packet loss"
```

### 5.2 Alert Manager Configuration

#### File: `monitoring/alertmanager.yml`
```yaml
global:
  resolve_timeout: 5m
  smtp_smarthost: 'smtp.example.com:587'
  smtp_from: 'alerts@example.com'
  smtp_auth_username: 'alerts@example.com'
  smtp_auth_password: 'password'

route:
  group_by: ['alertname', 'cluster', 'service']
  group_wait: 10s
  group_interval: 10s
  repeat_interval: 12h
  receiver: 'team-email'
  
  routes:
    # Critical alerts go to PagerDuty
    - match:
        severity: critical
      receiver: 'pagerduty'
      continue: true
    
    # All alerts go to email
    - match_re:
        severity: (critical|warning)
      receiver: 'team-email'
    
    # Slack notifications for warnings
    - match:
        severity: warning
      receiver: 'slack'

receivers:
  - name: 'team-email'
    email_configs:
      - to: 'nse-team@example.com'
        headers:
          Subject: '[{{ .Status | toUpper }}] {{ .GroupLabels.alertname }}'
  
  - name: 'pagerduty'
    pagerduty_configs:
      - service_key: 'YOUR_PAGERDUTY_KEY'
        description: '{{ .GroupLabels.alertname }}: {{ .CommonAnnotations.summary }}'
  
  - name: 'slack'
    slack_configs:
      - api_url: 'https://hooks.slack.com/services/YOUR/SLACK/WEBHOOK'
        channel: '#nse-alerts'
        title: '{{ .GroupLabels.alertname }}'
        text: '{{ .CommonAnnotations.description }}'
```

---

## Phase 6: Grafana Dashboards (Week 3)

### 6.1 Main Dashboard

**Panels:**

1. **Overview Row**
   - Uptime (gauge)
   - Packet Rate (graph)
   - Processing Rate (graph)
   - Error Rate (graph)

2. **Performance Row**
   - P50/P95/P99 Latency (graph)
   - Queue Depth (graph)
   - CPU Usage (gauge)
   - Memory Usage (gauge)

3. **Messages Row**
   - Messages by Type (pie chart)
   - Compression Ratio (graph)
   - Message Rate by Type (table)

4. **Errors Row**
   - Decompression Failures (graph)
   - Parsing Errors (graph)
   - Network Errors (graph)
   - Sequence Gaps (graph)

### 6.2 Dashboard JSON

See `monitoring/grafana_dashboard.json` (to be created)

---

## Implementation Checklist

### Week 1: Metrics Foundation
- [ ] Implement `Metrics` class
- [ ] Implement `HttpServer` class
- [ ] Add metrics collection to `MulticastReceiver`
- [ ] Add metrics collection to parsers
- [ ] Add metrics collection to LZO decompressor
- [ ] Test `/metrics` endpoint
- [ ] Test `/health` endpoint

### Week 2: Health & Prometheus
- [ ] Implement `HealthCheck` class
- [ ] Add health check logic
- [ ] Create Prometheus configuration
- [ ] Create recording rules
- [ ] Test Prometheus scraping
- [ ] Verify metrics in Prometheus UI

### Week 3: Alerting & Dashboards
- [ ] Create alert rules
- [ ] Configure AlertManager
- [ ] Test alert firing
- [ ] Create Grafana dashboard
- [ ] Test dashboard visualization
- [ ] Document runbooks

---

## Testing Strategy

### Unit Tests
```cpp
// tests/test_metrics.cpp
TEST(MetricsTest, CounterIncrement) {
    Metrics& m = Metrics::getInstance();
    m.reset();
    m.incrementCounter("test_counter", 5);
    // Verify counter value
}

TEST(MetricsTest, HistogramObservation) {
    Metrics& m = Metrics::getInstance();
    m.reset();
    m.observeHistogram("test_histogram", 100.0);
    // Verify histogram statistics
}
```

### Integration Tests
- Test metrics endpoint returns valid Prometheus format
- Test health endpoint returns valid JSON
- Test Prometheus can scrape metrics
- Test alerts fire correctly

### Load Tests
- Verify metrics performance under high load (100K pps)
- Ensure metrics collection doesn't impact latency
- Test HTTP server can handle concurrent requests

---

## Performance Considerations

### Metrics Collection Overhead
- Use atomic operations for counters/gauges
- Batch histogram observations
- Limit histogram bucket count
- Use lock-free data structures where possible

### HTTP Server Overhead
- Run in separate thread
- Use non-blocking I/O
- Limit concurrent connections
- Cache metrics export string

### Expected Impact
- CPU overhead: < 2%
- Memory overhead: < 10MB
- Latency impact: < 1μs per packet

---

## Deployment

### Prerequisites
```bash
# Install Prometheus
wget https://github.com/prometheus/prometheus/releases/download/v2.45.0/prometheus-2.45.0.linux-amd64.tar.gz
tar xvfz prometheus-*.tar.gz
cd prometheus-*

# Install Grafana
sudo apt-get install -y software-properties-common
sudo add-apt-repository "deb https://packages.grafana.com/oss/deb stable main"
sudo apt-get update
sudo apt-get install grafana

# Install AlertManager
wget https://github.com/prometheus/alertmanager/releases/download/v0.26.0/alertmanager-0.26.0.linux-amd64.tar.gz
tar xvfz alertmanager-*.tar.gz
```

### Configuration
```bash
# Copy configurations
cp monitoring/prometheus.yml /etc/prometheus/
cp monitoring/alert_rules.yml /etc/prometheus/
cp monitoring/recording_rules.yml /etc/prometheus/
cp monitoring/alertmanager.yml /etc/alertmanager/

# Start services
systemctl start prometheus
systemctl start grafana-server
systemctl start alertmanager
```

---

## Success Criteria

- ✅ All key metrics exposed via `/metrics`
- ✅ Health checks functional
- ✅ Prometheus successfully scraping metrics
- ✅ Alerts firing correctly
- ✅ Grafana dashboard showing real-time data
- ✅ < 2% CPU overhead from monitoring
- ✅ < 10MB memory overhead
- ✅ Documentation complete

---

## Future Enhancements

1. **Distributed Tracing** (OpenTelemetry)
2. **Log Aggregation** (ELK Stack)
3. **Anomaly Detection** (ML-based)
4. **Custom Dashboards** per team
5. **SLO/SLI Tracking**
6. **Capacity Planning** metrics

---

## References

- [Prometheus Documentation](https://prometheus.io/docs/)
- [Grafana Documentation](https://grafana.com/docs/)
- [AlertManager Documentation](https://prometheus.io/docs/alerting/latest/alertmanager/)
- [Prometheus Best Practices](https://prometheus.io/docs/practices/)

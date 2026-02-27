# Multi-Threading Architecture Design

## Overview

This document outlines the multi-threading architecture for the NSE UDP Reader to achieve higher throughput and better CPU utilization.

**Goals:**
- Process 100K+ packets per second
- Utilize multiple CPU cores
- Minimize latency
- Maintain thread safety

---

## Current Architecture (Single-Threaded)

```
┌─────────────┐
│   Main      │
│   Thread    │
│             │
│  ┌────────┐ │
│  │ Receive│ │  ← Blocks here
│  └────┬───┘ │
│       │     │
│  ┌────▼───┐ │
│  │Decomp  │ │
│  └────┬───┘ │
│       │     │
│  ┌────▼───┐ │
│  │ Parse  │ │
│  └────┬───┘ │
│       │     │
│  ┌────▼───┐ │
│  │ Stats  │ │
│  └────────┘ │
└─────────────┘
```

**Bottleneck**: Single thread handles all operations sequentially

---

## Proposed Architecture (Multi-Threaded)

### Design 1: Producer-Consumer with Lock-Free Queue

```
┌──────────────┐     ┌─────────────────┐     ┌──────────────┐
│   Receiver   │────▶│  Lock-Free      │────▶│  Processing  │
│   Thread     │     │  Queue          │     │  Thread Pool │
│              │     │  (Ring Buffer)  │     │  (4 threads) │
│  - recv()    │     │                 │     │              │
│  - enqueue   │     │  - SPSC/MPMC    │     │  - Decomp    │
│              │     │  - 1024 slots   │     │  - Parse     │
│              │     │                 │     │  - Stats     │
└──────────────┘     └─────────────────┘     └──────────────┘
```

**Advantages:**
- Receiver never blocks on processing
- Parallel decompression and parsing
- Better CPU utilization

**Challenges:**
- Queue overflow handling
- Memory management
- Thread synchronization

### Design 2: Pipeline Architecture

```
┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐
│ Receiver │──▶│ Decomp   │──▶│  Parse   │──▶│  Stats   │
│ Thread   │   │ Thread   │   │  Thread  │   │  Thread  │
│          │   │ Pool (2) │   │ Pool (2) │   │          │
└──────────┘   └──────────┘   └──────────┘   └──────────┘
     │              │              │              │
     ▼              ▼              ▼              ▼
  Queue 1       Queue 2       Queue 3       Results
```

**Advantages:**
- Clear separation of concerns
- Easy to scale each stage
- Predictable latency

**Challenges:**
- More complex
- More queues to manage
- Potential pipeline stalls

---

## Implementation Plan

### Phase 1: Lock-Free Queue (Week 1-2)

#### 1.1 Choose Queue Implementation
```cpp
// Option 1: Boost Lockfree
#include <boost/lockfree/spsc_queue.hpp>
boost::lockfree::spsc_queue<Packet*, 1024> packet_queue;

// Option 2: Custom Ring Buffer
template<typename T, size_t Size>
class RingBuffer {
    std::array<T, Size> buffer;
    std::atomic<size_t> write_pos{0};
    std::atomic<size_t> read_pos{0};
    
public:
    bool push(const T& item);
    bool pop(T& item);
    size_t size() const;
};
```

#### 1.2 Packet Pool
```cpp
class PacketPool {
    std::vector<Packet*> pool;
    std::atomic<size_t> next_free{0};
    
public:
    Packet* allocate();
    void deallocate(Packet* pkt);
};
```

### Phase 2: Receiver Thread (Week 2-3)

```cpp
class MulticastReceiver {
    std::thread receiver_thread;
    boost::lockfree::spsc_queue<Packet*> packet_queue;
    PacketPool packet_pool;
    std::atomic<bool> running{false};
    
    void receiver_loop() {
        while (running) {
            Packet* pkt = packet_pool.allocate();
            ssize_t n = recv(sockfd, pkt->data, BUFFER_SIZE, 0);
            
            if (n > 0) {
                pkt->size = n;
                if (!packet_queue.push(pkt)) {
                    // Queue full - drop packet
                    stats.queue_overflows++;
                    packet_pool.deallocate(pkt);
                }
            }
        }
    }
};
```

### Phase 3: Processing Thread Pool (Week 3-4)

```cpp
class ProcessingThread {
    boost::lockfree::spsc_queue<Packet*>& input_queue;
    PacketPool& packet_pool;
    std::atomic<bool>& running;
    
    void process_loop() {
        while (running) {
            Packet* pkt;
            if (input_queue.pop(pkt)) {
                // Process packet
                parse_packet(pkt);
                
                // Return to pool
                packet_pool.deallocate(pkt);
            } else {
                // Queue empty - yield
                std::this_thread::yield();
            }
        }
    }
};

class ThreadPool {
    std::vector<std::thread> workers;
    
public:
    void start(size_t num_threads) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back(&ProcessingThread::process_loop, ...);
        }
    }
};
```

### Phase 4: Statistics Aggregation (Week 4)

```cpp
class ThreadSafeStats {
    struct PerThreadStats {
        std::atomic<uint64_t> packets{0};
        std::atomic<uint64_t> bytes{0};
        // ... other counters
    };
    
    std::vector<PerThreadStats> thread_stats;
    
public:
    void record_packet(size_t thread_id, size_t bytes) {
        thread_stats[thread_id].packets++;
        thread_stats[thread_id].bytes += bytes;
    }
    
    UDPStats aggregate() const {
        UDPStats total;
        for (const auto& ts : thread_stats) {
            total.totalPackets += ts.packets.load();
            total.totalBytes += ts.bytes.load();
        }
        return total;
    }
};
```

---

## Performance Considerations

### Memory Management

#### Packet Pool Sizing
```cpp
// Calculate pool size
const size_t PACKET_RATE = 100000;  // packets/sec
const size_t PROCESSING_TIME_US = 100;  // microseconds
const size_t POOL_SIZE = (PACKET_RATE * PROCESSING_TIME_US) / 1000000 * 2;
// = 20 packets (with 2x safety margin)
```

#### Queue Sizing
```cpp
// Queue should handle burst traffic
const size_t BURST_DURATION_MS = 100;
const size_t QUEUE_SIZE = (PACKET_RATE * BURST_DURATION_MS) / 1000;
// = 10000 packets
```

### CPU Affinity

```cpp
void set_thread_affinity(std::thread& thread, int cpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    pthread_setaffinity_np(thread.native_handle(), 
                          sizeof(cpu_set_t), &cpuset);
}

// Pin receiver to CPU 0
set_thread_affinity(receiver_thread, 0);

// Pin workers to CPUs 1-4
for (size_t i = 0; i < workers.size(); ++i) {
    set_thread_affinity(workers[i], i + 1);
}
```

### Cache Optimization

```cpp
// Align structures to cache line (64 bytes)
struct alignas(64) Packet {
    char data[BUFFER_SIZE];
    size_t size;
    uint64_t timestamp;
};

// Avoid false sharing in atomic counters
struct alignas(64) AtomicCounter {
    std::atomic<uint64_t> value{0};
};
```

---

## Testing Strategy

### Unit Tests
```cpp
TEST(ThreadPoolTest, ProcessPackets) {
    ThreadPool pool(4);
    // ... test packet processing
}

TEST(RingBufferTest, ConcurrentAccess) {
    RingBuffer<int, 1024> buffer;
    // ... test concurrent push/pop
}
```

### Load Testing
```bash
# Generate synthetic traffic
./packet_generator --rate 100000 --duration 60

# Monitor performance
perf stat -e cache-misses,cache-references ./nse_reader
```

### Stress Testing
```bash
# Test queue overflow handling
./packet_generator --rate 200000 --burst

# Test thread pool scaling
for threads in 1 2 4 8; do
    NSE_WORKER_THREADS=$threads ./nse_reader
done
```

---

## Migration Path

### Step 1: Add Threading (Non-Breaking)
- Keep single-threaded mode as default
- Add `--threads N` command-line option
- Gradual rollout

### Step 2: Performance Validation
- Compare single vs multi-threaded
- Measure latency distribution
- Verify correctness

### Step 3: Make Default
- Switch default to multi-threaded
- Keep single-threaded as fallback
- Update documentation

---

## Expected Performance

### Baseline (Single-Threaded)
- Throughput: 10K-20K pps
- Latency: 50-100μs per packet
- CPU: 100% of 1 core

### Target (Multi-Threaded, 4 workers)
- Throughput: 100K+ pps
- Latency: 20-50μs per packet
- CPU: 50% of 4 cores (200% total)

### Scalability
```
Threads  | Throughput | Latency | CPU
---------|------------|---------|-----
1        | 15K pps    | 67μs    | 100%
2        | 35K pps    | 57μs    | 180%
4        | 80K pps    | 50μs    | 320%
8        | 120K pps   | 67μs    | 600%
```

---

## Risks and Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| Queue overflow | Packet loss | Monitor queue depth, alert on high watermark |
| Thread contention | Reduced performance | Use lock-free structures, minimize shared state |
| Memory leaks | System crash | Rigorous testing with valgrind, RAII patterns |
| Increased complexity | Bugs | Comprehensive unit tests, gradual rollout |
| Ordering issues | Incorrect stats | Use thread-local stats, aggregate periodically |

---

## Conclusion

Multi-threading will significantly improve throughput and CPU utilization. The lock-free queue approach provides the best balance of performance and complexity.

**Recommended**: Start with Design 1 (Producer-Consumer) and 4 worker threads.

/**
 * @file test_service_registry.cpp
 * @brief Unit tests for ServiceRegistry — DI container with LIFO destruction
 *
 * Tests:
 *  - Register / resolve by type
 *  - Unowned services (no auto-delete)
 *  - hasService check
 *  - Unregister with/without destruction
 *  - LIFO destruction order on shutdown()
 *  - Named services (multiple instances of same type)
 *  - clearAll (no destruction)
 *  - Overwrite warning
 *  - serviceCount / serviceNames
 *
 * Build: Requires Qt5::Core, Qt5::Test — header-only ServiceRegistry
 */

#include <QtTest/QtTest>
#include <QDebug>
#include "core/ServiceRegistry.h"

// ─── Test service types ──────────────────────────────────

class ITestService {
public:
    virtual ~ITestService() = default;
    virtual QString name() const = 0;
};

class ServiceA : public ITestService {
public:
    int* m_destructionFlag;
    ServiceA(int* flag = nullptr) : m_destructionFlag(flag) {}
    ~ServiceA() override { if (m_destructionFlag) *m_destructionFlag = 1; }
    QString name() const override { return "ServiceA"; }
};

class ServiceB : public ITestService {
public:
    int* m_destructionFlag;
    ServiceB(int* flag = nullptr) : m_destructionFlag(flag) {}
    ~ServiceB() override { if (m_destructionFlag) *m_destructionFlag = 1; }
    QString name() const override { return "ServiceB"; }
};

class ServiceC {
public:
    int value;
    int* m_destructionOrder;  // pointer to a counter
    ServiceC(int v, int* orderCounter = nullptr) : value(v), m_destructionOrder(orderCounter) {}
    ~ServiceC() {
        if (m_destructionOrder) {
            value = (*m_destructionOrder)++;  // record order of destruction
        }
    }
};

// ─── Test Class ──────────────────────────────────────────

class TestServiceRegistry : public QObject {
    Q_OBJECT

private slots:
    // Basic registration & resolution
    void testRegisterAndResolve();
    void testResolveUnregistered_returnsNull();
    void testHasService();
    void testServiceCount();
    void testServiceNames();

    // Ownership
    void testOwnedServiceDestroyed_onShutdown();
    void testUnownedServiceNotDestroyed_onShutdown();

    // Unregister
    void testUnregisterWithDestroy();
    void testUnregisterWithoutDestroy();

    // LIFO destruction
    void testDestructionOrder_LIFO();

    // Named services
    void testNamedServices();
    void testNamedServices_sameType();

    // Overwrite
    void testOverwriteExisting();

    // clearAll
    void testClearAll_noDestruction();

    // Resolve by interface
    void testResolveByInterface();

    // Multiple registries (for testing isolation)
    void testIsolatedRegistries();
};

// ─── Basic registration & resolution ─────────────────────

void TestServiceRegistry::testRegisterAndResolve() {
    ServiceRegistry reg;
    auto* svc = new ServiceA();
    reg.registerService<ServiceA>(svc);

    auto* resolved = reg.resolve<ServiceA>();
    QCOMPARE(resolved, svc);
    QCOMPARE(resolved->name(), QString("ServiceA"));
}

void TestServiceRegistry::testResolveUnregistered_returnsNull() {
    ServiceRegistry reg;
    auto* resolved = reg.resolve<ServiceA>();
    QCOMPARE(resolved, nullptr);
}

void TestServiceRegistry::testHasService() {
    ServiceRegistry reg;
    QVERIFY(!reg.hasService<ServiceA>());

    reg.registerService<ServiceA>(new ServiceA());
    QVERIFY(reg.hasService<ServiceA>());
    QVERIFY(!reg.hasService<ServiceB>());
}

void TestServiceRegistry::testServiceCount() {
    ServiceRegistry reg;
    QCOMPARE(reg.serviceCount(), 0);

    reg.registerService<ServiceA>(new ServiceA());
    QCOMPARE(reg.serviceCount(), 1);

    reg.registerService<ServiceB>(new ServiceB());
    QCOMPARE(reg.serviceCount(), 2);
}

void TestServiceRegistry::testServiceNames() {
    ServiceRegistry reg;
    reg.registerService<ServiceA>(new ServiceA(), "FeedHandler");
    reg.registerService<ServiceB>(new ServiceB(), "RepositoryManager");

    auto names = reg.serviceNames();
    QVERIFY(names.contains("FeedHandler"));
    QVERIFY(names.contains("RepositoryManager"));
}

// ─── Ownership ───────────────────────────────────────────

void TestServiceRegistry::testOwnedServiceDestroyed_onShutdown() {
    int destroyed = 0;

    {
        ServiceRegistry reg;
        reg.registerService<ServiceA>(new ServiceA(&destroyed));
        QCOMPARE(destroyed, 0);
    }
    // Registry destructor calls shutdown → deletes owned services
    QCOMPARE(destroyed, 1);
}

void TestServiceRegistry::testUnownedServiceNotDestroyed_onShutdown() {
    int destroyed = 0;
    ServiceA localService(&destroyed);

    {
        ServiceRegistry reg;
        reg.registerServiceUnowned<ServiceA>(&localService);
        QCOMPARE(destroyed, 0);
    }
    // Registry destructor should NOT delete unowned services
    QCOMPARE(destroyed, 0);
}

// ─── Unregister ──────────────────────────────────────────

void TestServiceRegistry::testUnregisterWithDestroy() {
    int destroyed = 0;
    ServiceRegistry reg;
    reg.registerService<ServiceA>(new ServiceA(&destroyed));

    reg.unregisterService<ServiceA>(true);
    QCOMPARE(destroyed, 1);
    QVERIFY(!reg.hasService<ServiceA>());
    QCOMPARE(reg.serviceCount(), 0);
}

void TestServiceRegistry::testUnregisterWithoutDestroy() {
    int destroyed = 0;
    auto* svc = new ServiceA(&destroyed);
    ServiceRegistry reg;
    reg.registerService<ServiceA>(svc);

    reg.unregisterService<ServiceA>(false);
    QCOMPARE(destroyed, 0);  // Not destroyed
    QVERIFY(!reg.hasService<ServiceA>());

    // Clean up manually
    delete svc;
}

// ─── LIFO Destruction ────────────────────────────────────

void TestServiceRegistry::testDestructionOrder_LIFO() {
    // Register A, B, C — should destroy C, B, A
    // We record destruction order via an external vector to avoid
    // reading dangling memory after delete.
    int orderCounter = 0;
    std::vector<std::pair<QString, int>> destructionLog;

    // Wrapper that records destruction order into destructionLog
    struct TrackedService {
        QString label;
        int* counter;
        std::vector<std::pair<QString, int>>* log;
        TrackedService(const QString& l, int* c, std::vector<std::pair<QString, int>>* lg)
            : label(l), counter(c), log(lg) {}
        ~TrackedService() {
            if (counter && log) {
                log->push_back({label, (*counter)++});
            }
        }
    };

    {
        ServiceRegistry reg;
        reg.registerService<TrackedService>(new TrackedService("A", &orderCounter, &destructionLog), "A");
        reg.registerService<TrackedService>(new TrackedService("B", &orderCounter, &destructionLog), "B");
        reg.registerService<TrackedService>(new TrackedService("C", &orderCounter, &destructionLog), "C");
    }
    // After shutdown: C destroyed first (order=0), B second (order=1), A third (order=2)
    QCOMPARE(orderCounter, 3);
    QCOMPARE(destructionLog.size(), (size_t)3);
    QCOMPARE(destructionLog[0].first, QString("C"));
    QCOMPARE(destructionLog[0].second, 0);
    QCOMPARE(destructionLog[1].first, QString("B"));
    QCOMPARE(destructionLog[1].second, 1);
    QCOMPARE(destructionLog[2].first, QString("A"));
    QCOMPARE(destructionLog[2].second, 2);
}

// ─── Named Services ─────────────────────────────────────

void TestServiceRegistry::testNamedServices() {
    ServiceRegistry reg;
    reg.registerService<ServiceA>(new ServiceA(), "primary");
    reg.registerService<ServiceB>(new ServiceB(), "secondary");

    auto* p = reg.resolve<ServiceA>("primary");
    auto* s = reg.resolve<ServiceB>("secondary");
    QVERIFY(p != nullptr);
    QVERIFY(s != nullptr);
    QCOMPARE(p->name(), QString("ServiceA"));
    QCOMPARE(s->name(), QString("ServiceB"));
}

void TestServiceRegistry::testNamedServices_sameType() {
    ServiceRegistry reg;
    auto* s1 = new ServiceA();
    auto* s2 = new ServiceA();
    reg.registerService<ServiceA>(s1, "instance1");
    reg.registerService<ServiceA>(s2, "instance2");

    QCOMPARE(reg.resolve<ServiceA>("instance1"), s1);
    QCOMPARE(reg.resolve<ServiceA>("instance2"), s2);
    QCOMPARE(reg.serviceCount(), 2);
}

// ─── Overwrite ───────────────────────────────────────────

void TestServiceRegistry::testOverwriteExisting() {
    int destroyed1 = 0;
    int destroyed2 = 0;
    ServiceRegistry reg;
    reg.registerService<ServiceA>(new ServiceA(&destroyed1), "svc");

    // Overwrite — old service IS auto-destroyed and its destruction entry is removed
    reg.registerService<ServiceA>(new ServiceA(&destroyed2), "svc");

    QCOMPARE(destroyed1, 1);  // Old service was destroyed on overwrite

    auto* resolved = reg.resolve<ServiceA>("svc");
    QVERIFY(resolved != nullptr);
    // The latest registration should be the one resolved
    QCOMPARE(destroyed2, 0);  // New service still alive

    // Verify only one destruction entry exists (no double-delete on shutdown)
    reg.shutdown();
    QCOMPARE(destroyed2, 1);  // Now destroyed via shutdown
}

// ─── clearAll ────────────────────────────────────────────

void TestServiceRegistry::testClearAll_noDestruction() {
    int destroyed = 0;
    auto* svc = new ServiceA(&destroyed);
    ServiceRegistry reg;
    reg.registerService<ServiceA>(svc);

    reg.clearAll();
    QCOMPARE(destroyed, 0);  // Not destroyed
    QCOMPARE(reg.serviceCount(), 0);
    QVERIFY(!reg.hasService<ServiceA>());

    delete svc;  // Manual cleanup
}

// ─── Resolve by interface ────────────────────────────────

void TestServiceRegistry::testResolveByInterface() {
    ServiceRegistry reg;
    auto* impl = new ServiceA();
    reg.registerService<ITestService>(impl);

    auto* resolved = reg.resolve<ITestService>();
    QVERIFY(resolved != nullptr);
    QCOMPARE(resolved->name(), QString("ServiceA"));
}

// ─── Isolated registries ────────────────────────────────

void TestServiceRegistry::testIsolatedRegistries() {
    ServiceRegistry reg1;
    ServiceRegistry reg2;

    // Register by type (not by name) to verify true isolation
    reg1.registerService<ServiceA>(new ServiceA());
    QVERIFY(reg1.hasService<ServiceA>());
    QVERIFY(!reg2.hasService<ServiceA>());  // Isolated — reg2 has nothing

    reg2.registerService<ServiceB>(new ServiceB());
    QVERIFY(!reg1.hasService<ServiceB>());  // reg1 doesn't have ServiceB
    QVERIFY(reg2.hasService<ServiceB>());

    // Double-check: each registry still only has its own service
    QCOMPARE(reg1.serviceCount(), 1);
    QCOMPARE(reg2.serviceCount(), 1);
}

// ─── Main ────────────────────────────────────────────────

QTEST_MAIN(TestServiceRegistry)
#include "test_service_registry.moc"

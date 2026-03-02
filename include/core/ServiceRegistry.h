#ifndef SERVICE_REGISTRY_H
#define SERVICE_REGISTRY_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QDebug>
#include <typeinfo>
#include <memory>
#include <mutex>
#include <functional>
#include <vector>

/**
 * @brief Centralized service registry with explicit ownership and lifetime management
 *
 * Replaces the ad-hoc singleton pattern with a structured dependency injection
 * container. Services are registered at startup and resolved by type.
 *
 * Benefits over raw singletons:
 *  - Deterministic destruction order (reverse of registration)
 *  - Testable: tests can register mocks instead of real services
 *  - Explicit dependencies: services declare what they need
 *  - Single place to audit all service lifetimes
 *
 * Usage:
 * ```cpp
 * // At startup (AppBootstrap):
 * auto& reg = ServiceRegistry::instance();
 * reg.registerService<FeedHandler>(new FeedHandler());
 * reg.registerService<RepositoryManager>(new RepositoryManager());
 *
 * // Anywhere in the codebase:
 * auto* feed = ServiceRegistry::instance().resolve<FeedHandler>();
 *
 * // In tests:
 * ServiceRegistry testReg;
 * testReg.registerService<IFeedHandler>(new MockFeedHandler());
 * auto* feed = testReg.resolve<IFeedHandler>();
 * ```
 *
 * Thread Safety:
 *  - Registration is NOT thread-safe (call only at startup)
 *  - Resolution IS thread-safe (lock-free after registration)
 */
class ServiceRegistry
{
public:
    /**
     * @brief Get the global singleton registry
     *
     * For production use. Tests should create local instances instead.
     */
    static ServiceRegistry& instance() {
        static ServiceRegistry s_instance;
        return s_instance;
    }

    ServiceRegistry() = default;

    /**
     * @brief Destroy all registered services in reverse order
     */
    ~ServiceRegistry() {
        shutdown();
    }

    // Non-copyable, non-movable
    ServiceRegistry(const ServiceRegistry&) = delete;
    ServiceRegistry& operator=(const ServiceRegistry&) = delete;

    /**
     * @brief Register a service instance
     *
     * The registry takes ownership and will delete the service on shutdown.
     * Registration order determines destruction order (LIFO).
     *
     * @tparam T The service type (interface or concrete class)
     * @param service Raw pointer to the service (registry takes ownership)
     * @param name Optional name override (defaults to typeid name)
     */
    template<typename T>
    void registerService(T* service, const QString& name = QString()) {
        QString key = name.isEmpty() ? QString::fromUtf8(typeid(T).name()) : name;

        if (m_services.contains(key)) {
            qWarning() << "[ServiceRegistry] Overwriting existing service:" << key;

            // Delete the old service and remove its destruction entry
            void* oldPtr = m_services[key];
            for (auto dit = m_destructionOrder.begin(); dit != m_destructionOrder.end(); ++dit) {
                if (dit->key == key) {
                    dit->destructor(oldPtr);
                    m_destructionOrder.erase(dit);
                    break;
                }
            }
        }

        m_services[key] = static_cast<void*>(service);

        // Record destruction order (type-erased destructor)
        m_destructionOrder.push_back({key, [](void* ptr) {
            delete static_cast<T*>(ptr);
        }});

        qDebug() << "[ServiceRegistry] Registered service:" << key;
    }

    /**
     * @brief Register a service without ownership transfer
     *
     * The registry will NOT delete this service on shutdown.
     * Use for services with external lifetime management (e.g., Qt parent-child).
     *
     * @tparam T The service type
     * @param service Raw pointer to the service
     * @param name Optional name override
     */
    template<typename T>
    void registerServiceUnowned(T* service, const QString& name = QString()) {
        QString key = name.isEmpty() ? QString::fromUtf8(typeid(T).name()) : name;

        if (m_services.contains(key)) {
            qWarning() << "[ServiceRegistry] Overwriting existing service:" << key;
        }

        m_services[key] = static_cast<void*>(service);
        // No destructor entry — not owned by registry

        qDebug() << "[ServiceRegistry] Registered unowned service:" << key;
    }

    /**
     * @brief Resolve a service by type
     *
     * @tparam T The service type to resolve
     * @param name Optional name override
     * @return Pointer to the service, or nullptr if not registered
     */
    template<typename T>
    T* resolve(const QString& name = QString()) const {
        QString key = name.isEmpty() ? QString::fromUtf8(typeid(T).name()) : name;

        auto it = m_services.find(key);
        if (it == m_services.end()) {
            qWarning() << "[ServiceRegistry] Service not found:" << key;
            return nullptr;
        }

        return static_cast<T*>(it.value());
    }

    /**
     * @brief Check if a service is registered
     */
    template<typename T>
    bool hasService(const QString& name = QString()) const {
        QString key = name.isEmpty() ? QString::fromUtf8(typeid(T).name()) : name;
        return m_services.contains(key);
    }

    /**
     * @brief Unregister and optionally destroy a single service
     *
     * @tparam T The service type
     * @param destroy If true, also delete the service
     * @param name Optional name override
     */
    template<typename T>
    void unregisterService(bool destroy = true, const QString& name = QString()) {
        QString key = name.isEmpty() ? QString::fromUtf8(typeid(T).name()) : name;

        auto it = m_services.find(key);
        if (it == m_services.end()) return;

        void* ptr = it.value();
        m_services.erase(it);

        // Remove from destruction order
        for (auto dit = m_destructionOrder.begin(); dit != m_destructionOrder.end(); ++dit) {
            if (dit->key == key) {
                if (destroy) {
                    dit->destructor(ptr);
                }
                m_destructionOrder.erase(dit);
                break;
            }
        }

        qDebug() << "[ServiceRegistry] Unregistered service:" << key;
    }

    /**
     * @brief Destroy all services in reverse registration order
     *
     * Called by destructor, but can also be called explicitly for
     * deterministic cleanup (e.g., before QApplication exits).
     */
    void shutdown() {
        if (m_destructionOrder.empty()) return;

        qDebug() << "[ServiceRegistry] Shutting down" << m_destructionOrder.size() << "services...";

        // Destroy in reverse order (LIFO)
        for (auto it = m_destructionOrder.rbegin(); it != m_destructionOrder.rend(); ++it) {
            auto svcIt = m_services.find(it->key);
            if (svcIt != m_services.end()) {
                qDebug() << "[ServiceRegistry] Destroying:" << it->key;
                it->destructor(svcIt.value());
            }
        }

        m_services.clear();
        m_destructionOrder.clear();

        qDebug() << "[ServiceRegistry] Shutdown complete";
    }

    /**
     * @brief Get count of registered services
     */
    int serviceCount() const { return m_services.size(); }

    /**
     * @brief Get list of registered service names
     */
    QStringList serviceNames() const { return m_services.keys(); }

    /**
     * @brief Clear all registrations without destroying services
     *
     * Useful in test teardown when services have external lifetime.
     */
    void clearAll() {
        m_services.clear();
        m_destructionOrder.clear();
    }

private:
    struct DestructionEntry {
        QString key;
        std::function<void(void*)> destructor;
    };

    QHash<QString, void*> m_services;
    std::vector<DestructionEntry> m_destructionOrder;
};

#endif // SERVICE_REGISTRY_H

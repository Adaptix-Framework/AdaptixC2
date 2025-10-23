#include "MCPCommandHandler.h"

// Static instance initialization
MCPCommandRegistry* MCPCommandRegistry::s_instance = nullptr;

MCPCommandRegistry* MCPCommandRegistry::instance() {
    if (!s_instance) {
        s_instance = new MCPCommandRegistry();
    }
    return s_instance;
}

MCPCommandRegistry::~MCPCommandRegistry() {
    clear();
}

void MCPCommandRegistry::registerHandler(const QString& type, IMCPCommandHandler* handler) {
    QMutexLocker locker(&mutex);
    
    if (handlers.contains(type)) {
        qWarning() << "[MCP Registry] Handler for type" << type << "already registered, replacing";
        delete handlers[type];
    }
    
    handlers[type] = handler;
    qDebug() << "[MCP Registry] Registered handler for command type:" << type;
}

IMCPCommandHandler* MCPCommandRegistry::getHandler(const QString& type) {
    QMutexLocker locker(&mutex);
    return handlers.value(type, nullptr);
}

bool MCPCommandRegistry::isSupported(const QString& type) {
    QMutexLocker locker(&mutex);
    IMCPCommandHandler* handler = handlers.value(type, nullptr);
    return handler != nullptr && handler->isSupported();
}

QStringList MCPCommandRegistry::getSupportedCommands() const {
    QMutexLocker locker(&mutex);
    QStringList result;
    
    for (auto it = handlers.constBegin(); it != handlers.constEnd(); ++it) {
        if (it.value()->isSupported()) {
            result.append(it.key());
        }
    }
    
    return result;
}

QList<MCP::Capability> MCPCommandRegistry::getAllCapabilities() const {
    QMutexLocker locker(&mutex);
    QList<MCP::Capability> result;
    
    for (auto handler : handlers) {
        result.append(handler->getCapability());
    }
    
    return result;
}

void MCPCommandRegistry::clear() {
    QMutexLocker locker(&mutex);
    
    qDebug() << "[MCP Registry] Clearing all handlers";
    
    for (auto handler : handlers) {
        delete handler;
    }
    
    handlers.clear();
}


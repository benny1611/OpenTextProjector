#pragma once
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <any>
#include <string>
#include <stdexcept>

// A lightweight Dependency Injection Container
class DIContainer {
private:
    std::unordered_map<std::type_index, std::any> _services;

public:
    // Registers a shared pointer as a Singleton
    template <typename T>
    void registerService(std::shared_ptr<T> service) {
        _services[typeid(T)] = service;
    }

    // Resolves and returns the requested Singleton
    template <typename T>
    std::shared_ptr<T> resolve() {
        auto it = _services.find(typeid(T));
        if (it != _services.end()) {
            return std::any_cast<std::shared_ptr<T>>(it->second);
        }
        throw std::runtime_error("DI Error: Service not found -> " + std::string(typeid(T).name()));
    }
};
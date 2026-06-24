#pragma once
#include <thread>
#include <atomic>
#include <memory>
#include <optional>
#include <Poco/Logger.h>
#include <Poco/JSON/Object.h>
#include "../Util/ThreadSafeQueue.h"
#include "../Models/Command.h"
#include "../Core/DIContainer.h"
#include "../System/TextManager.h"

class CommandProcessor {
public:
    CommandProcessor(std::shared_ptr<ThreadSafeQueue<Command>> commandQueue, std::shared_ptr<DIContainer> diContainer);
    ~CommandProcessor();

    void start();
    void stop();

private:
    void processLoop();
    void applyCommand(const Command& cmd);
    void handleText(Poco::JSON::Object::Ptr data);
    bool allManagersPresent();
    std::optional<int> resolveId(Poco::JSON::Object::Ptr data);
    template<typename T> std::optional<T> tryToGet(Poco::JSON::Object::Ptr object, const std::string& key);

    std::shared_ptr<ThreadSafeQueue<Command>> _commandQueue;
    std::shared_ptr<DIContainer> _diContainer;
    std::shared_ptr<TextManager> _textManager = nullptr;
    std::thread _workerThread;
    std::atomic<bool> _isRunning;
    Poco::Logger& _logger;
};
#pragma once
#include <thread>
#include <atomic>
#include <memory>
#include "../Util/ThreadSafeQueue.h"
#include "../Models/Command.h"

class CommandProcessor {
public:
    CommandProcessor(std::shared_ptr<ThreadSafeQueue<Command>> commandQueue);
    ~CommandProcessor();

    void start();
    void stop();

private:
    void processLoop();
    void applyCommand(const Command& cmd);

    std::shared_ptr<ThreadSafeQueue<Command>> _commandQueue;
    std::thread _workerThread;
    std::atomic<bool> _isRunning;
};
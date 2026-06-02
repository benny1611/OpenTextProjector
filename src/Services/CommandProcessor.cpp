#include "CommandProcessor.h"
#include "Poco/JSON/Array.h"
#include <iostream>

CommandProcessor::CommandProcessor(std::shared_ptr<ThreadSafeQueue<Command>> commandQueue)
    : _commandQueue(commandQueue), _isRunning(false) {
}

CommandProcessor::~CommandProcessor() {
    stop();
}

void CommandProcessor::start() {
    if (_isRunning) return;
    _isRunning = true;
    _workerThread = std::thread(&CommandProcessor::processLoop, this);
    std::cout << "[CommandProcessor] Background processing thread started." << std::endl;
}

void CommandProcessor::stop() {
    if (!_isRunning) return;
    _isRunning = false;

    // Push an empty dummy command to wake up the condition variable so it can exit
    _commandQueue->push(Command{ "SYSTEM", new Poco::JSON::Object() });

    if (_workerThread.joinable()) {
        _workerThread.join();
    }
    std::cout << "[CommandProcessor] Background processing thread stopped." << std::endl;
}

void CommandProcessor::processLoop() {
    while (_isRunning) {
        Command cmd;
        _commandQueue->wait_and_pop(cmd);

        if (!_isRunning || cmd.senderId == "SYSTEM") {
            break;
        }

        applyCommand(cmd);
    }
}

void CommandProcessor::applyCommand(const Command& cmd) {
    auto data = cmd.payload;
    if (!data) return;

    if (data->has("text")) {
        std::string text = data->getValue<std::string>("text");
        std::cout << "Handling text: " << text << std::endl;
    }

    if (data->has("font_size")) {
        int fontSize = data->getValue<int>("font_size");
        std::cout << "Setting font size to: " << fontSize << std::endl;
    }

    if (data->has("font_color")) {
        try {
            Poco::JSON::Array::Ptr colorArray = data->getArray("font_color");
            if (colorArray && colorArray->size() >= 3) {
                double r = colorArray->getElement<double>(0);
                double g = colorArray->getElement<double>(1);
                double b = colorArray->getElement<double>(2);
                std::cout << "Setting RGB to: " << r << ", " << g << ", " << b << std::endl;
            }
        }
        catch (const Poco::Exception& e) {
            std::cerr << "Failed to parse color array: " << e.displayText() << std::endl;
        }
    }
}
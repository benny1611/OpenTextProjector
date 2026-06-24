#include "CommandProcessor.h"
#include <Poco/JSON/Array.h>
#include <Poco/Base64Decoder.h>
#include <Poco/StreamCopier.h>
#include <Poco/Exception.h>
#include <iostream>
#include <sstream>
#include <filesystem>

CommandProcessor::CommandProcessor(std::shared_ptr<ThreadSafeQueue<Command>> commandQueue, std::shared_ptr<DIContainer> diContainer)
    : _commandQueue(commandQueue), _isRunning(false), _diContainer(diContainer), _logger(Poco::Logger::get("CommandProcessor")) {
}

CommandProcessor::~CommandProcessor() {
    stop();
}

void CommandProcessor::start() {
    if (_isRunning) return;
    _isRunning = true;
    _workerThread = std::thread(&CommandProcessor::processLoop, this);
    _logger.information("Background processing thread started.");
}

void CommandProcessor::stop() {
    if (!_isRunning) return;
    _isRunning = false;

    // Push an empty dummy command to wake up the condition variable so it can exit
    _commandQueue->push(Command{ "SYSTEM", new Poco::JSON::Object() });

    if (_workerThread.joinable()) {
        _workerThread.join();
    }
    _logger.information("Background processing thread stopped.");
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
    if (!allManagersPresent()) return;

    if (data->has("text")) {
        _logger.debug("Handling text");
        handleText(data);
    }
}

bool CommandProcessor::allManagersPresent() {
    bool result = true;

    if (_textManager == nullptr) {
        try {
            _textManager = _diContainer->resolve<TextManager>();
        }
        catch (std::runtime_error e) {
            _logger.error("Could not find the text manager");
            result = false;
        }
    }

    return result;
}

void CommandProcessor::handleText(Poco::JSON::Object::Ptr data) {
    Poco::JSON::Object::Ptr textObject;
    try {
        textObject = data->getObject("text");
    } catch (const Poco::Exception& e) {
        _logger.error("Could not parse the text object");
        return;
    }
    std::optional<int> idOptional = resolveId(textObject);
    if (!idOptional.has_value()) {
        _logger.error("Couldn't get or create the ID");
        return;
    }
    int id = idOptional.value();

    if (textObject->has("text")) {
        std::optional<std::string> base64StringOptional = tryToGet<std::string>(textObject, "text");
        if (base64StringOptional.has_value()) {
            try {
                std::string base64String = base64StringOptional.value();
                std::istringstream inStream(base64String);
                Poco::Base64Decoder decoder(inStream);
                std::ostringstream outStream;
                Poco::StreamCopier::copyStream(decoder, outStream);
                std::string utf8String = outStream.str();

                _logger.debug("Handling text: %s", utf8String);
                _textManager->setText(id, utf8String);
            } catch (const Poco::Exception& e) {
                _logger.error("POCO Exception: %s", e.displayText());
            } catch (const std::exception& e) {
                _logger.error("Standard exception: %s", e.what());
            }
        }
    }

    if (textObject->has("font_size")) {
        std::optional<double> fontSizeOptional = tryToGet<double>(textObject, "font_size");
        if (fontSizeOptional.has_value()) {
            double fontSize = fontSizeOptional.value();
            _logger.debug("Setting font size to: " + std::to_string(fontSize));
            _textManager->setFontSize(id, fontSize);
        }
    }

    if (textObject->has("font_color")) {
        try {
            Poco::JSON::Array::Ptr colorArray = textObject->getArray("font_color");
            if (colorArray && colorArray->size() >= 4) {
                double r = colorArray->getElement<double>(0);
                double g = colorArray->getElement<double>(1);
                double b = colorArray->getElement<double>(2);
                double a = colorArray->getElement<double>(3);
                _logger.debug("Setting font color to: R: %f, G: %f, B: %f, A: %f", r, g, b, a);
                _textManager->setFontColor(id, r, g, b, a);
            }
        }
        catch (const Poco::Exception& e) {
            _logger.error("Failed to parse color array: %s", e.displayText());
        }
    }

    if (textObject->has("debug")) {
        std::optional<bool> debugOptional = tryToGet<bool>(textObject, "debug");
        if (debugOptional.has_value()) {
            bool debug = debugOptional.value();
            _textManager->setDebugMode(id, debug);
        }
    }

    if (textObject->has("font")) {
        std::optional<std::string> fontNameOptional = tryToGet<std::string>(textObject, "font");
        if (fontNameOptional.has_value()) {
            std::string fontName = fontNameOptional.value();

            // Construct the expected path
            std::filesystem::path fontPath = "resources/fonts/" + fontName + ".ttf";

            // Check if the file exists and is an actual file (not a directory)
            if (std::filesystem::exists(fontPath) && std::filesystem::is_regular_file(fontPath)) {
                _logger.debug("Font found: %s", fontPath.string());
                _textManager->setFont(id, fontPath.string());
            }
            else {
                _logger.error("Font file does not exist at: %s", fontPath.string());
            }
        }
    }

    if (textObject->has("z_index")) {
        std::optional<int> zIndexOptional = tryToGet<int>(textObject, "z_index");
        if (zIndexOptional.has_value()) {
            int zIndex = zIndexOptional.value();
            if (zIndex >= 0) {
                _logger.debug("Setting z index of %d to %d", id, zIndex);
                _textManager->setZIndex(id, zIndex);
            } else {
                _logger.error("zIndex must be positive");
            }
        }
    }
}

template<typename T> std::optional<T> CommandProcessor::tryToGet(Poco::JSON::Object::Ptr object, const std::string& key) {
    if (!object || !object->has(key))
        return std::nullopt;

    try {
        return object->getValue<T>(key);
    } catch (const Poco::Exception&) {
        return std::nullopt;
    }
}

std::optional<int> CommandProcessor::resolveId(Poco::JSON::Object::Ptr textObject) {
    int id;
    if (textObject->has("id")) {
        std::optional<int> idOptional = tryToGet<int>(textObject, "id");
        if (idOptional.has_value()) {
            id = idOptional.value();
            if (!_textManager->hasId(id)) {
                _logger.error("id value does not exist");
                return std::nullopt;
            }
            _logger.debug("Hanlding id: %d", id);
            return std::optional<int>(id);
        }
        else {
            _logger.error("id value could not be parsed");
            return std::nullopt;
        }
    }
    else {
        _logger.debug("Creating new text box");
        // check given coordinates
        float x;
        float y;
        if (textObject->has("box_position")) {
            try {
                Poco::JSON::Array::Ptr boxPositionArray = textObject->getArray("box_position");
                if (boxPositionArray->size() != 2) {
                    _logger.error("box position array must have exactly 2 elements: x and y");
                    return std::nullopt;
                }
                x = boxPositionArray->getElement<float>(0);
                y = boxPositionArray->getElement<float>(1);
            }
            catch (const Poco::Exception& e) {
                _logger.error("Error while pasing the box position array: " + e.message());
                return std::nullopt;
            }
        } else {
            _logger.error("No id given and box position not found");
            return std::nullopt;
        }
        // check the given box size
        float width;
        float height;
        if (textObject->has("box_size")) {
            try {
                Poco::JSON::Array::Ptr boxSizeArray = textObject->getArray("box_size");
                if (boxSizeArray->size() != 2) {
                    _logger.error("box size must have exactly 2 elements: width and height");
                    return std::nullopt;
                }
                width = boxSizeArray->getElement<float>(0);
                height = boxSizeArray->getElement<float>(1);
            }
            catch (const Poco::Exception& e) {
                _logger.error("Error while parsing the box size: " + e.message());
                return std::nullopt;
            }
        } else {
            _logger.error("No id given and box size not found");
            return std::nullopt;
        }

        id = _textManager->createTextBox(x, y, width, height);
        return std::optional<int>(id);
    }
}
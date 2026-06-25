#include "TextManager.h"
#include <iostream>
#include <algorithm>
#include <filesystem>

TextManager::TextManager() : _logger(Poco::Logger::get("TextManager")) {
    if (FT_Init_FreeType(&ftLibrary)) {
        _logger.critical("CRITICAL ERROR: Failed initialization of FreeType pipeline reference.");
        exit(1);
    }
}

TextManager::~TextManager() {
    for (auto& pair : loadedFaces) {
        FT_Done_Face(pair.second);
    }
    FT_Done_FreeType(ftLibrary);
}

FT_Face TextManager::getFaceHandle(const std::string& fontPath) {
    if (loadedFaces.find(fontPath) != loadedFaces.end()) {
        return loadedFaces[fontPath];
    }

    FT_Face newFace;
    if (FT_New_Face(ftLibrary, fontPath.c_str(), 0, &newFace)) {
        _logger.error("ERROR: FreeType failed reading font system target: %s", fontPath);
        return nullptr;
    }
    loadedFaces[fontPath] = newFace;
    return newFace;
}

int TextManager::createTextBox(float x, float y, float width, float height) {
    std::lock_guard<std::mutex> lock(managerMutex);
    int id = nextId++;
    auto renderer = std::make_unique<TextRenderer>(id);
    renderer->setBox(x, y, width, height);
    textRenderers[id] = std::move(renderer);
    _logger.debug("Created text box at x: %.2f, y: %.2f, width: %.2f, height: %.2f, id: %d", 
        static_cast<double>(x), 
        static_cast<double>(y), 
        static_cast<double>(width), 
        static_cast<double>(height), id);
    return id;
}

void TextManager::removeTextBox(int id) {
    std::lock_guard<std::mutex> lock(managerMutex);
    textRenderers.erase(id);
}

// Thread-safe command routing endpoints
void TextManager::setText(int id, const std::string& text) {
    std::lock_guard<std::mutex> lock(managerMutex);
    if (textRenderers.find(id) != textRenderers.end()) {
        if (textRenderers[id]->getText() != text) {
            textRenderers[id]->setText(text);
            _logger.debug("@%d: Text set to: %s", id, text);
        }
    }
}

void TextManager::setBoxDimensions(int id, float x, float y, float width, float height) {
    std::lock_guard<std::mutex> lock(managerMutex);
    if (textRenderers.find(id) != textRenderers.end()) textRenderers[id]->setBox(x, y, width, height);
}

void TextManager::setFontColor(int id, float r, float g, float b, float a) {
    std::lock_guard<std::mutex> lock(managerMutex);
    if (textRenderers.find(id) != textRenderers.end()) {
        Color c = textRenderers[id]->getColor();
        if (c.r != r || c.g != g || c.b != b || c.a != a) {
            textRenderers[id]->setColor(r, g, b, a);
            _logger.debug("@%d: color set to: R: %.2f, G: %.2f, B: %.2f, A: %.2f",
                id,
                static_cast<double>(r),
                static_cast<double>(g),
                static_cast<double>(b),
                static_cast<double>(a));
        }
    }
}

void TextManager::setFont(int id, const std::string& fontName) {
    std::lock_guard<std::mutex> lock(managerMutex);
    if (textRenderers.find(id) != textRenderers.end()) {
        // Construct the expected path
        std::filesystem::path newFontPath = "resources/fonts/" + fontName + ".ttf";
        std::string newFontPathString = newFontPath.string();
        if (std::filesystem::exists(newFontPath) && std::filesystem::is_regular_file(newFontPath)) {
            std::string currentFontPath = textRenderers[id]->getFontPath();
            if (newFontPathString != currentFontPath) {
                textRenderers[id]->setFontPath(newFontPathString);
                _logger.debug("@%d: Font path set to: %s", id, newFontPathString);
            }
        }
    }
}

void TextManager::setFontSize(int id, float desiredSize, float decreaseStep) {
    std::lock_guard<std::mutex> lock(managerMutex);
    if (textRenderers.find(id) != textRenderers.end()) {
        if (textRenderers[id]->getFontSize() != desiredSize) {
            textRenderers[id]->setFontSize(desiredSize, decreaseStep);
            _logger.debug("@%d: Set font size to: %.2f, decrease step: %.2f", id, static_cast<double>(desiredSize), static_cast<double>(decreaseStep));
        }
    }
}

void TextManager::setLineSpacing(int id, float spacing) {
    std::lock_guard<std::mutex> lock(managerMutex);
    if (textRenderers.find(id) != textRenderers.end()) textRenderers[id]->setLineSpacing(spacing);
}

void TextManager::setWordWrap(int id, bool wrap) {
    std::lock_guard<std::mutex> lock(managerMutex);
    if (textRenderers.find(id) != textRenderers.end()) textRenderers[id]->setWordWrap(wrap);
}

void TextManager::setAlignment(int id, TextAlignment alignment) {
    std::lock_guard<std::mutex> lock(managerMutex);
    if (textRenderers.find(id) != textRenderers.end()) textRenderers[id]->setAlignment(alignment);
}

void TextManager::setZIndex(int id, int zIndex) {
    std::lock_guard<std::mutex> lock(managerMutex);
    if (textRenderers.find(id) != textRenderers.end()) {
        int currentZIndex = textRenderers[id]->getZIndex();
        if (currentZIndex != zIndex) {
            textRenderers[id]->setZIndex(zIndex);
            _logger.debug("@%d: Set Z index to: %d", id, zIndex);
        }
    }
}

void TextManager::setDebugMode(int id, bool showDebug) {
    std::lock_guard<std::mutex> lock(managerMutex);
    if (textRenderers.find(id) != textRenderers.end()) {
        bool debug = textRenderers[id]->getDebugLines();
        if (debug != showDebug) {
            textRenderers[id]->setDebugLines(showDebug);
            _logger.debug("@%d: set debug to: %s", id, std::string(showDebug ? "true" : "false"));
        }
    }
}

bool TextManager::hasId(int id) {
    std::lock_guard<std::mutex> lock(managerMutex);
    return textRenderers.find(id) != textRenderers.end();
}

void TextManager::renderAll() {
    std::lock_guard<std::mutex> lock(managerMutex);

    // Establish dynamic layout matrix projections tailored around viewport details
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, viewport[2], viewport[3], 0.0, -1.0, 1.0); // Coordinates top-left origin maps

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Construct a sorted execution render order track via local container indexing
    std::vector<TextRenderer*> sortedRenderers;
    for (auto& pair : textRenderers) {
        sortedRenderers.push_back(pair.second.get());
    }

    std::sort(sortedRenderers.begin(), sortedRenderers.end(), [](const TextRenderer* a, const TextRenderer* b) {
        return a->getZIndex() < b->getZIndex();
        });

    // Rebuild fonts metrics & fire the execution drawing pass
    for (auto* renderer : sortedRenderers) {
        FT_Face face = getFaceHandle(renderer->getFontPath());
        if (face) {
            renderer->rebuildLayoutIfNeeded(face);
            renderer->draw();
        }
    }

    // Reset fixed-function matrix environments back safely
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}
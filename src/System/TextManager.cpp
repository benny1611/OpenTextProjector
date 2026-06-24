#include "TextManager.h"
#include <iostream>
#include <algorithm>

TextManager::TextManager() {
    if (FT_Init_FreeType(&ftLibrary)) {
        std::cerr << "CRITICAL ERROR: Failed initialization of FreeType pipeline reference." << std::endl;
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
        std::cerr << "ERROR: FreeType failed reading font system target: " << fontPath << std::endl;
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
        }
    }
}

void TextManager::setFont(int id, const std::string& fontPath) {
    std::lock_guard<std::mutex> lock(managerMutex);
    if (textRenderers.find(id) != textRenderers.end()) {
        std::string currentFont = textRenderers[id]->getFontPath();
        if (currentFont != fontPath) {
            textRenderers[id]->setFontPath(fontPath);
        }
    }
}

void TextManager::setFontSize(int id, float desiredSize, float decreaseStep) {
    std::lock_guard<std::mutex> lock(managerMutex);
    if (textRenderers.find(id) != textRenderers.end()) {
        if (textRenderers[id]->getFontSize() != desiredSize) {
            textRenderers[id]->setFontSize(desiredSize, decreaseStep);
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
        }
    }
}

void TextManager::setDebugMode(int id, bool showDebug) {
    std::lock_guard<std::mutex> lock(managerMutex);
    if (textRenderers.find(id) != textRenderers.end()) {
        bool debug = textRenderers[id]->getDebugLines();
        if (debug != showDebug) {
            textRenderers[id]->setDebugLines(showDebug);
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
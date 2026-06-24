#pragma once
#include "TextRenderer.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include <string>
#include <ft2build.h>
#include FT_FREETYPE_H

class TextManager {
public:
    TextManager();
    ~TextManager();

    // Standard allocation interface for incoming client configurations
    int createTextBox(float x, float y, float width, float height);
    void removeTextBox(int id);

    // Completely Thread-Safe Setter Facades for external pipeline workers
    void setText(int id, const std::string& text);
    void setBoxDimensions(int id, float x, float y, float width, float height);
    void setFontColor(int id, float r, float g, float b, float a);
    void setFont(int id, const std::string& fontPath);
    void setFontSize(int id, float desiredSize, float decreaseStep = 2.0f);
    void setLineSpacing(int id, float spacing);
    void setWordWrap(int id, bool wrap);
    void setAlignment(int id, TextAlignment alignment);
    void setZIndex(int id, int zIndex);
    void setDebugMode(int id, bool showDebug);
    bool hasId(int id);

    // Frame execution loop target. Must only run on the Main OpenGL Loop context.
    void renderAll();

private:
    FT_Library ftLibrary;
    std::unordered_map<std::string, FT_Face> loadedFaces;
    std::unordered_map<int, std::unique_ptr<TextRenderer>> textRenderers;

    std::mutex managerMutex;
    int nextId = 1;

    FT_Face getFaceHandle(const std::string& fontPath);
};
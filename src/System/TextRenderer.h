#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <ft2build.h>
#include <tuple>
#include FT_FREETYPE_H

enum class TextAlignment {
    Left,
    Center,
    Right
};

struct Color {
    Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}
    float r;
    float g;
    float b;
    float a;
};

struct Character {
    GLuint TextureID;   // Glyph texture coordinate pointer
    glm::ivec2 Size;    // Width and height metrics of glyph
    glm::ivec2 Bearing; // Offset metrics from baseline to left/top
    GLuint Advance;     // Horizontal stride offset to next glyph
};

struct TextLine {
    std::vector<unsigned long> codepoints;
    float width = 0.0f;
};

class TextRenderer {
public:
    explicit TextRenderer(int id);
    ~TextRenderer();

    // Cleans up associated OpenGL textures safely
    void clearCache();

    // Layout calculation and texture binding (Must run on the main thread)
    void rebuildLayoutIfNeeded(FT_Face face);

    
    void draw();

    // Thread-safe data setting modifications (Triggers dirty state)
    void setText(const std::string& newText);
    void setBoxDimensions(float x, float y, float width, float height);
    void setBoxPosition(float x, float y);
    void setBoxSize(float width, float height);
    void setColor(float r, float g, float b, float a);
    void setFontSize(float desiredSize, float decreaseStep);
    void setLineSpacing(float spacing);
    void setWordWrap(bool wrap);
    void setFontPath(const std::string& path);
    void setAlignment(TextAlignment align);
    void setZIndex(int z);
    void setDebugLines(bool drawDebug);


    // Accessors
    int getId() const { return id; }
    int getZIndex() const { return zIndex; }
    float getFontSize() const { return desiredFontSize; }
    bool getDebugLines() const { return drawDebugLines; }
    std::string getFontPath() const { return fontPath; }
    std::string getText() const { return text; }
    Color getColor() const { return Color(colorR, colorG, colorB, colorA); }
    std::tuple<float, float> getBoxPosition() { return std::make_tuple(boxX, boxY); }
    std::tuple<float, float> getBoxSize() { return std::make_tuple(boxWidth, boxHeight); }

private:
    int id;

    // Layout configuration variables
    float boxX = 0.0f, boxY = 0.0f;
    float boxWidth = 200.0f, boxHeight = 100.0f;

    std::string text;
    std::string fontPath = "resources/fonts/Raleway.ttf";

    float desiredFontSize = 32.0f;
    float actualFontSize = 32.0f;
    float fontDecreaseStep = 2.0f;
    float lineSpacing = 1.2f;

    float colorR = 1.0f, colorG = 1.0f, colorB = 1.0f, colorA = 1.0f;
    bool wordWrap = true;
    bool drawDebugLines = false;
    TextAlignment alignment = TextAlignment::Left;
    int zIndex = 0;

    bool isDirty = true;

    // Generated layout rendering lines and local texture cache
    std::vector<TextLine> lines;
    std::unordered_map<unsigned long, Character> glyphCache;
};
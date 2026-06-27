#include "TextRenderer.h"
#include <Poco/UTF8Encoding.h>
#include <Poco/TextIterator.h>
#include <algorithm>

TextRenderer::TextRenderer(int id) : id(id) {}

TextRenderer::~TextRenderer() {
    clearCache();
}

void TextRenderer::clearCache() {
    for (auto& pair : glyphCache) {
        glDeleteTextures(1, &pair.second.TextureID);
    }
    glyphCache.clear();
}

void TextRenderer::rebuildLayoutIfNeeded(FT_Face face) {
    if (!isDirty) return;

    // Step 1: Wipe older allocations out to ensure clear resolution metrics
    clearCache();
    lines.clear();

    actualFontSize = desiredFontSize;

    // Step 2: Auto-Shrink Engine Loop
    while (actualFontSize > 6.0f) {
        FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(actualFontSize));
        lines.clear();

        struct Token {
            std::vector<unsigned long> codepoints;
            float width = 0.0f;
            bool isNewline = false;
            bool isSpace = false;
        };

        std::vector<Token> tokens;
        Token curToken;

        Poco::UTF8Encoding utf8;
        Poco::TextIterator it(text, utf8);
        Poco::TextIterator end(text);

        while (it != end) {
            unsigned long cp = *it;
            if (cp == '\n') {
                if (!curToken.codepoints.empty()) { 
                    tokens.push_back(curToken); curToken = Token(); 
                }
                Token nl; 
                nl.isNewline = true;
                tokens.push_back(nl);
            }
            else if (cp == ' ') {
                if (!curToken.codepoints.empty()) { 
                    tokens.push_back(curToken); curToken = Token(); 
                }
                Token sp; 
                sp.isSpace = true; 
                sp.codepoints.push_back(cp);
                if (!FT_Load_Char(face, cp, FT_LOAD_DEFAULT)) {
                    sp.width = static_cast<float>(face->glyph->advance.x >> 6);
                }
                tokens.push_back(sp);
            }
            else {
                curToken.codepoints.push_back(cp);
                if (!FT_Load_Char(face, cp, FT_LOAD_DEFAULT)) {
                    curToken.width += static_cast<float>(face->glyph->advance.x >> 6);
                }
            }
            ++it;
        }
        if (!curToken.codepoints.empty()) {
            tokens.push_back(curToken);
        }

        // Process word wrapping arrangement constraints
        TextLine currentLine;
        bool singleWordTooWide = false;

        for (const auto& token : tokens) {
            if (token.width > boxWidth && !token.isSpace) {
                singleWordTooWide = true;
            }

            if (token.isNewline) {
                lines.push_back(currentLine);
                currentLine = TextLine();
            }
            else {
                if (wordWrap && (currentLine.width + token.width > boxWidth) && !currentLine.codepoints.empty()) {
                    lines.push_back(currentLine);
                    currentLine = TextLine();
                    if (!token.isSpace) {
                        currentLine.codepoints.insert(currentLine.codepoints.end(), token.codepoints.begin(), token.codepoints.end());
                        currentLine.width += token.width;
                    }
                }
                else {
                    if (currentLine.codepoints.empty() && token.isSpace) continue; // Skip leading spaces
                    currentLine.codepoints.insert(currentLine.codepoints.end(), token.codepoints.begin(), token.codepoints.end());
                    currentLine.width += token.width;
                }
            }
        }
        if (!currentLine.codepoints.empty() || lines.empty()) {
            lines.push_back(currentLine);
        }

        // Validate vertical and horizontal clearance space
        float totalHeight = lines.size() * (actualFontSize * lineSpacing);
        if (totalHeight <= boxHeight && !singleWordTooWide) {
            break; // Text completely fits inside bounds
        }

        actualFontSize -= fontDecreaseStep;
    }

    // Step 3: Populate crisp textures exclusively for rendering components
    FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(actualFontSize));
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (const auto& line : lines) {
        for (unsigned long cp : line.codepoints) {
            if (glyphCache.find(cp) != glyphCache.end()) continue;

            if (FT_Load_Char(face, cp, FT_LOAD_RENDER)) continue;

            GLuint texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);

            glTexImage2D(
                GL_TEXTURE_2D, 0, GL_ALPHA,
                face->glyph->bitmap.width, face->glyph->bitmap.rows,
                0, GL_ALPHA, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer
            );

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            Character character = {
                texture,
                glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                static_cast<GLuint>(face->glyph->advance.x)
            };
            glyphCache[cp] = character;
        }
    }

    isDirty = false;
}

void TextRenderer::draw() {
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(colorR, colorG, colorB, colorA);

    float currentY = boxY + actualFontSize; // Drop down to initial layout baseline

    for (const auto& line : lines) {
        float currentX = boxX;
        if (alignment == TextAlignment::Center) {
            currentX = boxX + (boxWidth - line.width) / 2.0f;
        }
        else if (alignment == TextAlignment::Right) {
            currentX = boxX + boxWidth - line.width;
        }

        for (unsigned long cp : line.codepoints) {
            auto it = glyphCache.find(cp);
            if (it == glyphCache.end()) continue;
            const Character& ch = it->second;

            float xpos = currentX + ch.Bearing.x;
            float ypos = currentY - ch.Bearing.y;
            float w = static_cast<float>(ch.Size.x);
            float h = static_cast<float>(ch.Size.y);

            glBindTexture(GL_TEXTURE_2D, ch.TextureID);
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex2f(xpos, ypos);
            glTexCoord2f(1.0f, 0.0f); glVertex2f(xpos + w, ypos);
            glTexCoord2f(1.0f, 1.0f); glVertex2f(xpos + w, ypos + h);
            glTexCoord2f(0.0f, 1.0f); glVertex2f(xpos, ypos + h);
            glEnd();

            currentX += static_cast<float>(ch.Advance >> 6);
        }
        currentY += (actualFontSize * lineSpacing);
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    if (drawDebugLines) {
        glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(boxX, boxY);
        glVertex2f(boxX + boxWidth, boxY);
        glVertex2f(boxX + boxWidth, boxY + boxHeight);
        glVertex2f(boxX, boxY + boxHeight);
        glEnd();
    }
}

// Setter implementations targeting the conditional dirty state update
void TextRenderer::setText(const std::string& newText) { if (text != newText) { text = newText; isDirty = true; } }
void TextRenderer::setBoxDimensions(float x, float y, float width, float height) { boxX = x; boxY = y; boxWidth = width; boxHeight = height; isDirty = true; }
void TextRenderer::setBoxPosition(float x, float y) { boxX = x; boxY = y; isDirty = true; }
void TextRenderer::setBoxSize(float width, float height) { boxWidth = width; boxHeight = height; isDirty = true; }
void TextRenderer::setColor(float r, float g, float b, float a) { colorR = r; colorG = g; colorB = b; colorA = a; isDirty = true; }
void TextRenderer::setFontSize(float desiredSize, float decreaseStep) { desiredFontSize = desiredSize; fontDecreaseStep = decreaseStep; isDirty = true; }
void TextRenderer::setLineSpacing(float spacing) { lineSpacing = spacing; isDirty = true; }
void TextRenderer::setWordWrap(bool wrap) { wordWrap = wrap; isDirty = true; }
void TextRenderer::setFontPath(const std::string& path) { if (fontPath != path) { fontPath = path; isDirty = true; } }
void TextRenderer::setAlignment(TextAlignment align) { alignment = align; isDirty = true; }
void TextRenderer::setZIndex(int z) { zIndex = z; isDirty = true; }
void TextRenderer::setDebugLines(bool drawDebug) { drawDebugLines = drawDebug; isDirty = true; }
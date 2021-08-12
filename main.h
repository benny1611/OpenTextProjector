#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

struct Resolution {
    int width;
    int height;
};

struct Monitor {
    Resolution maxResolution;
    GLFWmonitor* monitor;
    const char* name;
};

struct Character {
	GLuint     TextureID;  // ID handle of the glyph texture
	glm::ivec2 Size;       // Size of glyph
	glm::ivec2 Bearing;    // Offset from baseline to left/top of glyph
	GLuint     Advance;    // Offset to advance to next glyph
};

#endif // MAIN_H_INCLUDED

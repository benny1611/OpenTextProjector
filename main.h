#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

#include <GL/glew.h>
#define GLFW_DLL
#include<GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext.hpp>

struct Resolution {
    int width;
    int height;
};

struct Monitor {
    Resolution maxResolution;
    GLFWmonitor* monitor;
    const char* name;
    int refreshRate;
};

struct Character {
	GLuint     TextureID;  // ID handle of the glyph texture
	glm::ivec2 Size;       // Size of glyph
	glm::ivec2 Bearing;    // Offset from baseline to left/top of glyph
	GLuint     Advance;    // Offset to advance to next glyph
};

#endif // MAIN_H_INCLUDED

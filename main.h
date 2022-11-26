#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

#include <GL/glew.h>
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext.hpp>

#include "tools/tinythread.h"
#include "tools/RTSPServer.hpp"

// concurrency stuff
extern tthread::mutex monitor_event_mutex;
extern tthread::mutex rtsp_server_mutex;
extern tthread::mutex text_mutex;
extern volatile char watchVariable;

// flags
extern bool RTSP_SERVER_SHOULD_START;
extern bool RTSP_SERVER_STARTED;
extern bool SHOULD_CHANGE_FONT;
extern bool SHOULD_CHANGE_FONT_COLOR;
extern bool SHOULD_CHANGE_MONITOR;
extern bool readyToSetFrame;

// variables
extern char* FONT;
extern std::list<std::wstring> TEXT;
extern float RED;
extern float GREEN;
extern float BLUE;
extern int FONT_SIZE;
extern int TOTAL_MONITORS;
extern int MONITOR_TO_CHANGE;
extern GLfloat TEXT_SCALE;
extern GLuint PADDING;
extern OTPRTSPServer* rtspServer;
extern std::string fontPath;



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

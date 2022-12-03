#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES

#include <freetype/ft2build.h>
#include FT_FREETYPE_H

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <map>
#include <iterator>
#include <fstream>
#include <asio/ip/tcp.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "main.h"
#include "tools/json.hpp"
#include "tools/base64.h"
#include "tools/ScreenSource.hpp"
#include "tools/TCPServer.h"
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>

using std::map;
using asio::ip::tcp;
using json = nlohmann::json;
using namespace std;
GLFWwindow* WINDOW;
void getMonitors(GLFWmonitor** monitors, Monitor* choiceMonitors);
void monitor_callback(GLFWmonitor* monitor, int event);
void rtspScreenShot(void * aArg);
void startServer(void * aArg);

Monitor DEFAULT_MONITOR;
GLfloat TEXT_X;
GLfloat TEXT_Y;
GLuint TEXT_WIDTH;
GLuint TEXT_HEIGHT;
Monitor *CHOICE_MONITORS;
bool monitor_event = false;
tthread::thread* rtsp;
tthread::thread* serverScreenShots;
tthread::mutex position_mutex;
tthread::mutex screenshot_mutex;
map<wchar_t, Character> Characters;
GLuint buffer;
bool rewriteLogFile = true;
FILE *pPipe;
bool send_screenshot = false;
UsageEnvironment* env;
int tcpServerPort = 8080;

// global variables for concurrency
tthread::mutex monitor_event_mutex = tthread::mutex();
tthread::mutex rtsp_server_mutex = tthread::mutex();
tthread::mutex text_mutex = tthread::mutex();
volatile char watchVariable;

// global flags
bool RTSP_SERVER_SHOULD_START = false;
bool RTSP_SERVER_STARTED = false;
bool SHOULD_CHANGE_FONT = false;
bool SHOULD_CHANGE_FONT_COLOR = false;
bool SHOULD_CHANGE_MONITOR = false;
bool readyToSetFrame = false;

// global variables
vector<wstring> TEXT;
char* FONT;
float RED = 1.0;
float GREEN = 1.0;
float BLUE = 1.0;
GLfloat TEXT_SCALE;
GLuint PADDING;
int FONT_SIZE;
int TOTAL_MONITORS;
int MONITOR_TO_CHANGE;
OTPRTSPServer* rtspServer;
string fontPath = "./fonts/";

void checkForErrors(string flag) {
    int glError = glGetError();
    if (glError) {
        printf("Here is your err(%s): %d\n", flag, glError);
    }
}

int main(int argc, char* argv[]) {
    // Start the TCP Server
    TCPServer* tcpServer = new TCPServer(tcpServerPort);
    tcpServer->start();
    // GLFW stuff
    if (!glfwInit()) {
        return -1;
    }
    if (!glewInit()) {
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_SAMPLES, 4);
    // keep fullscreen on focus
    glfwWindowHint(GLFW_AUTO_ICONIFY, GL_FALSE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    // Look for available monitors
    GLFWmonitor** monitors = glfwGetMonitors(&TOTAL_MONITORS);
    Monitor choiceMonitors[2] = {};
    getMonitors(monitors, choiceMonitors);

    for (int i=0; i<TOTAL_MONITORS; i++) {
        printf("\nMonitor %d found. Name: %s; Resolution: %d X %d\n", i, choiceMonitors[i].name, choiceMonitors[i].maxResolution.height, choiceMonitors[i].maxResolution.width);
    }

    glfwSetMonitorCallback(monitor_callback);

    CHOICE_MONITORS = choiceMonitors;
    DEFAULT_MONITOR = CHOICE_MONITORS[0];
    MONITOR_TO_CHANGE = 0;
    TEXT_SCALE = 1.0f;
    TEXT.push_back(L"WELCOME TO");
    TEXT.push_back(L"OpenTextProjector");
    TEXT.push_back(L"Made with love by benny1611");
    TEXT.push_back(L"ășîâțȚȘÂĂÎÖöÄäÜü");
    TEXT_WIDTH = 0;
    TEXT_HEIGHT = 0;
    FONT_SIZE = 50;
    FONT = "./fonts/raleway.ttf";
    PADDING = 5;

    printf("Done!\n");
    WINDOW = glfwCreateWindow(DEFAULT_MONITOR.maxResolution.width, DEFAULT_MONITOR.maxResolution.height, "OpenTextProjector", DEFAULT_MONITOR.monitor, nullptr);
    glfwMakeContextCurrent(WINDOW);
    glViewport(0, 0, DEFAULT_MONITOR.maxResolution.width, DEFAULT_MONITOR.maxResolution.height);

    glfreetype::font_data the_font;
    the_font.init(FONT, FONT_SIZE);
    
    list<int> width_list;
    int itr = 0;
	while (!glfwWindowShouldClose(WINDOW)) {
        monitor_event_mutex.lock();
        if (monitor_event) {
            monitor_event = false;
        }
        if (SHOULD_CHANGE_MONITOR) {
            SHOULD_CHANGE_MONITOR = false;
            DEFAULT_MONITOR = CHOICE_MONITORS[MONITOR_TO_CHANGE];
            glfwSetWindowMonitor(WINDOW, DEFAULT_MONITOR.monitor, 0, 0, DEFAULT_MONITOR.maxResolution.width, DEFAULT_MONITOR.maxResolution.height, DEFAULT_MONITOR.refreshRate);
        }
        monitor_event_mutex.unlock();

        rtsp_server_mutex.lock();
        if (RTSP_SERVER_SHOULD_START) {
            RTSP_SERVER_SHOULD_START = false;
            serverScreenShots = new tthread::thread(rtspScreenShot, 0);
            rtsp = new tthread::thread(startServer, 0);
            rtsp->detach();
            cout << "Detached ..." << endl;
            serverScreenShots->detach();
        }
        rtsp_server_mutex.unlock();


        text_mutex.lock();
        if(SHOULD_CHANGE_FONT) {
            SHOULD_CHANGE_FONT =  false;
            //initializeFreeType();
        }
        if(SHOULD_CHANGE_FONT_COLOR) {
            SHOULD_CHANGE_FONT_COLOR = false;
            glUniform3f(6, RED, GREEN, BLUE);
        }
        text_mutex.unlock();

        /*DRAWING NOW*/
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glPushMatrix();
        glLoadIdentity();
        glColor3ub(0xff, 0xff, 0xff);
        glfreetype::print(the_font, 300, 400, TEXT);
        glPopMatrix();
        glfwSwapBuffers(WINDOW);
        glfwPollEvents();
        // Screenshot:
        screenshot_mutex.lock();
        rtsp_server_mutex.lock();
        if(send_screenshot && readyToSetFrame) {
            //cout << "setting screenshot to false" << endl;
            send_screenshot = false;
            // Make the BYTE array, factor of 3 because it's RBG.
            std::vector<uint8_t> pixels(3 * DEFAULT_MONITOR.maxResolution.width * DEFAULT_MONITOR.maxResolution.height);
            glReadPixels(0, 0, DEFAULT_MONITOR.maxResolution.width, DEFAULT_MONITOR.maxResolution.height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            // flip the image
            for (int line = 0; line != DEFAULT_MONITOR.maxResolution.height / 2; ++line) {
                std::swap_ranges(pixels.begin() + 3 * DEFAULT_MONITOR.maxResolution.width * line,
                    pixels.begin() + 3 * DEFAULT_MONITOR.maxResolution.width * (line + 1),
                    pixels.begin() + 3 * DEFAULT_MONITOR.maxResolution.width * (DEFAULT_MONITOR.maxResolution.height - line - 1));
            }
            rtspServer->streamImage(pixels.data(), 0);
        }
        rtsp_server_mutex.unlock();
        screenshot_mutex.unlock();
    }
	printf("%d", glGetError());

    the_font.clean();
    glfwTerminate();
    return 0;
}

void rtspScreenShot(void * aArg) {
    while(true) {
        rtsp_server_mutex.lock();
        if(!RTSP_SERVER_STARTED) {
            rtsp_server_mutex.unlock();
            break;
        }
        rtsp_server_mutex.unlock();
        tthread::this_thread::sleep_for(tthread::chrono::milliseconds(33));
        screenshot_mutex.lock();
        //cout << "setting screenshot to true" << endl;
        send_screenshot = true;
        screenshot_mutex.unlock();
    }
}

void startServer(void * aArg) {
    rtspServer = new OTPRTSPServer(554, 8554);

    if (!rtspServer->init(DEFAULT_MONITOR.maxResolution.width, DEFAULT_MONITOR.maxResolution.height, 30, "opentextprojector")) {
        cerr << "Could not start the RTSP Server" << endl;
        exit(1);
    }

    rtspServer->doEvent(readyToSetFrame, &watchVariable);
}



void getMonitors(GLFWmonitor** monitors, Monitor* choiceMonitors) {
    printf("\nDetecting monitors ...\n\n");
    map<GLFWmonitor*, GLFWvidmode> monitorsModeMap;

    for(int currMonitor=0;currMonitor<TOTAL_MONITORS;currMonitor++)
    {
        int count;
        const GLFWvidmode* modes = glfwGetVideoModes(monitors[currMonitor], &count);

        Resolution maxResolution;
        maxResolution.height = 0;
        maxResolution.width = 0;

        for (int i = 0; i < count; i++)
        {
            if (modes[i].width * modes[i].height > maxResolution.width * maxResolution.height) {
                maxResolution.width = modes[i].width;
                maxResolution.height = modes[i].height;
                monitorsModeMap[monitors[currMonitor]] = modes[i];
            }
        }

    }

    int i = 0;
    for(auto it=monitorsModeMap.begin(); it!=monitorsModeMap.end(); ++it) {
        const char* name = glfwGetMonitorName(it->first);
        choiceMonitors[i].maxResolution.height = it->second.height;
        choiceMonitors[i].maxResolution.width = it->second.width;
        choiceMonitors[i].monitor = it->first;
        choiceMonitors[i].name = name;
        choiceMonitors[i].refreshRate = it->second.refreshRate;
        i++;
    }
}



void monitor_callback(GLFWmonitor* monitor, int event) {
    monitor_event_mutex.lock();
    if (event == GLFW_DISCONNECTED || event == GLFW_CONNECTED) {
        GLFWmonitor** monitors = glfwGetMonitors(&TOTAL_MONITORS);
        Monitor choiceMonitors[2] = {};
        getMonitors(monitors, choiceMonitors);
        CHOICE_MONITORS = choiceMonitors;
        DEFAULT_MONITOR = CHOICE_MONITORS[0];
        MONITOR_TO_CHANGE = 0;
        glfwSetWindowMonitor(WINDOW, DEFAULT_MONITOR.monitor, 0, 0, DEFAULT_MONITOR.maxResolution.width, DEFAULT_MONITOR.maxResolution.height, DEFAULT_MONITOR.refreshRate);
        monitor_event = true;
    }
    monitor_event_mutex.unlock();
}


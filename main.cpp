#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES

#include <GL/glew.h>
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext.hpp>
#include <freetype/ft2build.h>
#include FT_FREETYPE_H

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <map>
#include <iterator>
#include <fstream>
#include <asio/asio.hpp>
#include "main.h"
#include "tools/tinythread.h"
#include "tools/json.hpp"
#include "tools/base64.h"
#include "ScreenSource.hh"
#include <liveMedia/liveMedia.hh>
#include <BasicUsageEnvironment/BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>


using std::map;
using asio::ip::tcp;
using json = nlohmann::json;
using namespace std;

GLuint CompileShaders(bool vs_b, bool tcs_b, bool tes_b, bool gs_b, bool fs_b);
void getMonitors(GLFWmonitor** monitors, Monitor* choiceMonitors);
string read_(tcp::socket & socket);
list<wstring> getTextFromCommand(json command);
list<string> split(string str, char del);
vector<string> splitVector(string strToSplit, char delimeter);
void monitor_callback(GLFWmonitor* monitor, int event);
void setupMonitor();
void glSetup();
void centerText(int rows, int row);
void listen_for_connection(void* aArg);
void rtspScreenShot(void * aArg);
void play();
void afterPlaying(void*);
void startServer(void * aArg);
void initializeFreeType();
void APIENTRY GLDebugMessageCallback(GLenum source, GLenum type, GLuint id,GLenum severity, GLsizei length,const GLchar* msg, const void* data) {
	printf("%d: %s, severity: %d\n",id, msg, severity);
}
void log(string message);


Monitor DEFAULT_MONITOR;
GLfloat TEXT_X;
GLfloat TEXT_Y;
GLuint TEXT_WIDTH;
GLuint TEXT_HEIGHT;
GLuint PADDING;
GLfloat TEXT_SCALE;
list<wstring> TEXT;
int FONT_SIZE;
Monitor *CHOICE_MONITORS;
char* FONT;
int TOTAL_MONITORS;
bool SHOULD_CHANGE_MONITOR = false;
bool SHOULD_CHANGE_FONT = false;
bool SHOULD_CHANGE_FONT_COLOR = false;
bool RTSP_SERVER_STARTED = false;
bool RTSP_SERVER_SHOULD_START = false;
float RED = 1.0;
float GREEN = 1.0;
float BLUE = 1.0;
int MONITOR_TO_CHANGE;
GLFWwindow* WINDOW;
tthread::mutex monitor_event_mutex;
bool monitor_event = false;
tthread::mutex text_mutex;
tthread::mutex position_mutex;
tthread::mutex rtsp_server_mutex;
tthread::mutex screenshot_mutex;
map<wchar_t, Character> Characters;
GLuint buffer;
bool rewriteLogFile = true;
tthread::thread* rtsp;
FILE *pPipe;
bool send_screenshot = false;
UsageEnvironment* env;
H264VideoStreamDiscreteFramer* videoSource;
RTPSink* videoSink;
ScreenSource* screenSrc;
char volatile* watchVariable = 0;
bool readyToSetFrame = false;
RTSPServer* rtspServer;



int main(int argc, char *argv[]) {
    // GLFW stuff
    tthread::thread t(listen_for_connection, 0);
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    // Look for available monitors
    GLFWmonitor** monitors = glfwGetMonitors(&TOTAL_MONITORS);
    Monitor choiceMonitors[TOTAL_MONITORS] = {};
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
    TEXT_WIDTH = 0;
    TEXT_HEIGHT = 0;
    FONT_SIZE = 50;
    FONT = "./fonts/raleway.ttf";
    PADDING = 5;

    printf("Done!\n");

    setupMonitor();
    list<int> width_list;
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
            tthread::thread serverScreenShots(rtspScreenShot, 0);
            tthread::thread startRTSPServer(startServer, 0);
            serverScreenShots.detach();
            startRTSPServer.detach();
        }
        rtsp_server_mutex.unlock();


        text_mutex.lock();
        if(SHOULD_CHANGE_FONT) {
            SHOULD_CHANGE_FONT =  false;
            initializeFreeType();
        }
        if(SHOULD_CHANGE_FONT_COLOR) {
            SHOULD_CHANGE_FONT_COLOR = false;
            glUniform3f(6, RED, GREEN, BLUE);
        }
        GLuint globalHeight = 0;
        list<wstring>::iterator t;
        width_list.clear();
        for (t = TEXT.begin(); t != TEXT.end(); ++t) {
            TEXT_WIDTH = 0;
            TEXT_HEIGHT = 0;
            wstring::iterator c;
            for (c = (*t).begin(); c != (*t).end(); c++) {
                Character ch = Characters[*c];
                switch(*c) {
                    case 537: // ș
                        ch = Characters[351];
                        break;
                    case 536: // Ș
                        ch = Characters[350];
                        break;
                    case 539: // ț
                        ch = Characters[355];
                        break;
                    case 538: // Ț
                        ch = Characters[354];
                        break;
                }
                GLfloat w = ch.Size.x * TEXT_SCALE;
                GLfloat h = ch.Size.y * TEXT_SCALE;

                TEXT_WIDTH += w;
                if (h > TEXT_HEIGHT) {
                    TEXT_HEIGHT = h;
                }
            }
            width_list.push_back(TEXT_WIDTH);
            if (TEXT_HEIGHT > globalHeight) {
                globalHeight = TEXT_HEIGHT;
            }
        }

        /*DRAWING NOW*/
        int row = 1;
        GLfloat x = TEXT_X;
        GLfloat y = TEXT_Y;
        TEXT_HEIGHT = globalHeight + PADDING;
        glClear(GL_COLOR_BUFFER_BIT);
        for (t = TEXT.begin(); t != TEXT.end(); ++t) {
            list<int>::iterator it = width_list.begin();
            advance(it, row-1);
            TEXT_WIDTH = *it;
            centerText(TEXT.size(), row);
            row++;
            x = TEXT_X;
            y = TEXT_Y;
            wstring::iterator c;
            for (c = (*t).begin(); c != (*t).end(); c++) {
                Character ch = Characters[*c];
                switch(*c) {
                    case 537: // ș
                        ch = Characters[351];
                        break;
                    case 536: // Ș
                        ch = Characters[350];
                        break;
                    case 539: // ț
                        ch = Characters[355];
                        break;
                    case 538: // Ț
                        ch = Characters[354];
                        break;
                }
                GLfloat w = ch.Size.x * TEXT_SCALE;
                GLfloat h = ch.Size.y * TEXT_SCALE;

                GLfloat xpos = x + ch.Bearing.x * TEXT_SCALE;
                GLfloat ypos = y - (ch.Size.y - ch.Bearing.y) * TEXT_SCALE;


                // Update VBO for each character
                GLfloat vertices[6*4] = {
                     xpos,     ypos + h,   0.0f, 0.0f ,
                     xpos,     ypos,       0.0f, 1.0f ,
                     xpos + w, ypos,       1.0f, 1.0f ,

                     xpos,     ypos + h,   0.0f, 0.0f ,
                     xpos + w, ypos,       1.0f, 1.0f ,
                     xpos + w, ypos + h,   1.0f, 0.0f
                };

                glNamedBufferSubData(buffer, 0, sizeof(GLfloat)*6*4, vertices);
                glBindTexture(GL_TEXTURE_2D, ch.TextureID);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                x += (ch.Advance >> 6) * TEXT_SCALE;
            }
            text_mutex.unlock();
        }
        glfwSwapBuffers(WINDOW);
        glfwPollEvents();

        // Screenshot:
        screenshot_mutex.lock();
        if(send_screenshot && readyToSetFrame) {
            //cout << "setting screenshot to false" << endl;
            send_screenshot = false;
            screenSrc->frameMutex.lock();
            // Make the BYTE array, factor of 3 because it's RBG.
            BYTE* pixels = new BYTE[3 * DEFAULT_MONITOR.maxResolution.width * DEFAULT_MONITOR.maxResolution.height];
            //glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, DEFAULT_MONITOR.maxResolution.width, DEFAULT_MONITOR.maxResolution.height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
            screenSrc->setNextFrame(pixels, DEFAULT_MONITOR.maxResolution.width, DEFAULT_MONITOR.maxResolution.height);
            screenSrc->frameMutex.unlock();
            // Convert to FreeImage format & save to file
            /*FIBITMAP* image = FreeImage_ConvertFromRawBits(pixels, DEFAULT_MONITOR.maxResolution.width, DEFAULT_MONITOR.maxResolution.height, 3 * DEFAULT_MONITOR.maxResolution.width, 24, 0x0000FF, 0xFF0000, 0x00FF00, false);

            FreeImageIO io;
            ImageIOUtils::SetDefaultIO(&io);

            screenSrc->setFrame(image);

            // Free resources
            FreeImage_Unload(image);*/
            delete [] pixels;
        }
        screenshot_mutex.unlock();
    }
	printf("%d", glGetError());

    glfwTerminate();
    return 0;
}

void listen_for_connection(void* aArg) {
    while (true) {
        try {
            asio::io_context io_context;

            tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 8080));

            tcp::socket socket(io_context);

            acceptor.accept(socket);

            for (;;)
            {
                //screenshot_mutex.lock();
                // JSON command
                string message = read_(socket);
                json command = json::parse(message);
                //get the font
                try {
                    string font = command["font"].get<string>();
                    ifstream ifile;
                    ifile.open("./fonts/" + font + ".ttf");
                    if (ifile) { // only change font if the font exists
                        text_mutex.lock();
                        ifile.close();
                        font = "./fonts/" + font + ".ttf";
                        if (string(FONT) != font) {
                            FONT = strcpy((char*)malloc(font.length()+1), font.c_str());
                            SHOULD_CHANGE_FONT = true;
                        }
                        text_mutex.unlock();
                    }
                } catch (exception& e) {
                    cerr << "Error while trying to get the font: " << e.what() << endl;
                }
                // get the font color
                try {
                    vector<float> color = command["font_color"].get<vector<float>>();
                    if (color.size() != 3) {
                        goto skipColor;
                    }
                    float r, g, b;
                    r = color[0];
                    g = color[1];
                    b = color[2];
                    if (r == RED && g == GREEN && b == BLUE) {
                        goto skipColor;
                    }
                    text_mutex.lock();
                    RED = r;
                    GREEN = g;
                    BLUE = b;
                    SHOULD_CHANGE_FONT_COLOR = true;
                    text_mutex.unlock();
                } catch(exception& e) {
                    cerr << "Error while trying to get the font color: " << e.what() << endl;
                }
                skipColor:
                // get the text to be shown
                try {
                    list<wstring> result = getTextFromCommand(command);
                    text_mutex.lock();
                    TEXT.clear();
                    for (wstring s: result) {
                        TEXT.push_back(s);
                    }
                    text_mutex.unlock();
                } catch (exception& e) {
                    cerr << "Error while trying to get the text: " << e.what() << endl;
                }
                // get the text size
                try {
                    int font_size = command["font_size"].get<int>();
                    text_mutex.lock();
                    TEXT_SCALE = ((float)font_size) / ((float)FONT_SIZE);
                    text_mutex.unlock();
                } catch (exception& e) {
                    cerr << "Error while trying to get the font size: " << e.what() << endl;
                }
                try {
                    int monitor = command["monitor"].get<int>();
                    if (monitor < TOTAL_MONITORS && monitor >= 0 && monitor != MONITOR_TO_CHANGE) {
                        monitor_event_mutex.lock();
                        SHOULD_CHANGE_MONITOR = true;
                        MONITOR_TO_CHANGE = monitor;
                        monitor_event_mutex.unlock();
                    }
                } catch (exception& e) {
                    cerr << "Error while trying to get the monitor: " << e.what() << endl;
                }
                try {
                    string server = command["server"].get<string>();
                    rtsp_server_mutex.lock();
                    if (server == "start" && !RTSP_SERVER_STARTED) {
                        cout << "Starting the server..." << endl;
                        RTSP_SERVER_STARTED = true;
                        RTSP_SERVER_SHOULD_START = true;
                    } else if (server == "stop" && RTSP_SERVER_STARTED) {
                        cout << "Closing server ..." << endl;
                        RTSP_SERVER_STARTED = false;
                        watchVariable = (char*)1;
                        rtsp_server_mutex.unlock();
                    } else {
                        cerr << "Error: command \"" << server << "\" not valid. Is server currently running: " << RTSP_SERVER_STARTED << endl;
                    }
                    rtsp_server_mutex.unlock();
                } catch (exception& e) {
                    cerr << "Error while trying to get the server command: " << e.what() << endl;
                }
                //screenshot_mutex.unlock();
            }
        } catch (exception& e) {
            cerr << "Another exception has occurred: " << e.what() << endl;
        }
    }
}


void rtspScreenShot(void * aArg) {
    while(true) {
        rtsp_server_mutex.lock();
        if(!RTSP_SERVER_STARTED) {
            cout << "Breaking free now XD" << endl;
            rtsp_server_mutex.unlock();
            break;
        }
        rtsp_server_mutex.unlock();
        tthread::this_thread::sleep_for(tthread::chrono::milliseconds(100));
        screenshot_mutex.lock();
        //cout << "setting screenshot to true" << endl;
        send_screenshot = true;
        screenshot_mutex.unlock();
    }
    cout << "Done with this thread XD" << endl;
}

void play() {
    FramedSource* videoES = screenSrc;
    videoSource = H264VideoStreamDiscreteFramer::createNew(*env, videoES, False);
    *env << "Started streaming...\n";
    videoSink->startPlaying(*videoSource, afterPlaying, videoSink);
    cout << "Started playing! :)" << endl;
}

void afterPlaying(void*) {
    cout << "Playing again" << endl;
    play();
}

void startServer(void * aArg) {
    watchVariable = 0;
    const unsigned estimatedSessionBandwidth = 1024;
    const unsigned maxCNAMElen = 100;
    unsigned char CNAME[maxCNAMElen+1];
    char volatile* watchVariable = 0;
    const unsigned short rtpPortNum = 18888;
    const unsigned short rtcpPortNum = rtpPortNum + 1;

    // Live555 stuff
    // Begin by setting up the usage environment
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();

    env = BasicUsageEnvironment::createNew(*scheduler);


    // Create 'groupsocks' for RTP and RTCP:
    struct sockaddr_storage destinationAddress;
    destinationAddress.ss_family = AF_INET;
    ((struct sockaddr_in&)destinationAddress).sin_addr.s_addr = chooseRandomIPv4SSMAddress(*env);
    // Note: This is a multicast address.  If you wish instead to stream
    // using unicast, then you should use the "testOnDemandRTSPServer"
    // test program - not this test program - as a model.

    const unsigned char ttl = 255;
    const Port rtpPort(rtpPortNum);
    const Port rtcpPort(rtcpPortNum);

    Groupsock rtpGroupsock(*env, destinationAddress, rtpPort, ttl);
    rtpGroupsock.multicastSendOnly(); // we're a SSM source
    Groupsock rtcpGroupsock(*env, destinationAddress, rtcpPort, ttl);
    rtcpGroupsock.multicastSendOnly(); // we're a SSM source

    OutPacketBuffer::maxSize = 1000000;
    videoSink = H264VideoRTPSink::createNew(*env, &rtpGroupsock, 96);
    gethostname((char*)CNAME, maxCNAMElen);
    CNAME[maxCNAMElen] = '\0';
    screenSrc = ScreenSource::createNew(*env);
    if (screenSrc == NULL) {
        *env << "Unable to create a screen source";
        exit(1);
    }
    readyToSetFrame = true;
    RTCPInstance* rtcp
    = RTCPInstance::createNew(*env, &rtcpGroupsock,
                estimatedSessionBandwidth, CNAME,
                videoSink, NULL /* we're a server */,
                True /* we're a SSM source */);
    // Note: This starts RTCP running automatically
    rtspServer = RTSPServer::createNew(*env, 554);
    if (rtspServer == NULL) {
        *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
        exit(1);
    }

    ServerMediaSession* sms
    = ServerMediaSession::createNew(*env, "ipcamera","UPP Buffer" ,
           "Session streamed by \"testH264VideoStreamer\"",
                       True /*SSM*/);
    sms->addSubsession(PassiveServerMediaSubsession::createNew(*videoSink, rtcp));
    rtspServer->addServerMediaSession(sms);

    char* url = rtspServer->rtspURL(sms);
    *env << "Play this stream using the URL \"" << url << "\"\n";
    delete[] url;

    // Start the streaming:
    *env << "Beginning streaming...\n";
    play();
    if (rtspServer->setUpTunnelingOverHTTP(80) || rtspServer->setUpTunnelingOverHTTP(8000) || rtspServer->setUpTunnelingOverHTTP(8080)) {
        *env << "\n(We use port " << rtspServer->httpServerPortNum() << " for optional RTSP-over-HTTP tunneling.)\n";
    } else {
        *env << "\n(RTSP-over-HTTP tunneling is not available.)\n";
    }
    env->taskScheduler().doEventLoop(watchVariable); // does not return
    cout << "Terminated the server thread";
}

list<wstring> getTextFromCommand(json command) {
    string base64String = command["text"].get<string>();
    string result64 = base64_decode(base64String, false);
    vector<string> text_list = splitVector(result64, '\n');
    list<wstring> results;
    for (string t: text_list) {
        wstring result = L"";
        for(int i=0; i<t.length(); i++) {
            if((unsigned int)t[i] == 4294967236 && (unsigned int) t[i+1] == 4294967171) {
                result += L"ă";
                i++;
            } else if((unsigned int)t[i] == 4294967235 && (unsigned int) t[i+1] == 4294967214) {
                result += L"î";
                i++;
            } else if((unsigned int)t[i] == 4294967235 && (unsigned int) t[i+1] == 4294967202) {
                result += L"â";
                i++;
            } else if((unsigned int)t[i] == 4294967240 && (unsigned int) t[i+1] == 4294967193) {
                result += L"ș";
                i++;
            } else if((unsigned int)t[i] == 4294967240 && (unsigned int) t[i+1] == 4294967195) {
                result += L"ț";
                i++;
            } else if((unsigned int)t[i] == 4294967240 && (unsigned int) t[i+1] == 4294967194) {
                result += L"Ț";
                i++;
            } else if((unsigned int)t[i] == 4294967240 && (unsigned int) t[i+1] == 4294967192) {
                result += L"Ș";
                i++;
            } else if((unsigned int)t[i] == 4294967235 && (unsigned int) t[i+1] == 4294967170) {
                result += L"Â";
                i++;
            } else if((unsigned int)t[i] == 4294967236 && (unsigned int) t[i+1] == 4294967170) {
                result += L"Ă";
                i++;
            } else if((unsigned int)t[i] == 4294967235 && (unsigned int) t[i+1] == 4294967182) {
                result += L"Î";
                i++;
            } else if((unsigned int)t[i] == 4294967235 && (unsigned int) t[i+1] == 4294967190) {
                result += L"Ö";
                i++;
            } else if((unsigned int)t[i] == 4294967235 && (unsigned int) t[i+1] == 4294967222) {
                result += L"ö";
                i++;
            } else if((unsigned int)t[i] == 4294967235 && (unsigned int) t[i+1] == 4294967172) {
                result += L"Ä";
                i++;
            } else if((unsigned int)t[i] == 4294967235 && (unsigned int) t[i+1] == 4294967204) {
                result += L"ä";
                i++;
            } else if((unsigned int)t[i] == 4294967235 && (unsigned int) t[i+1] == 4294967196) {
                result += L"Ü";
                i++;
            } else if((unsigned int)t[i] == 4294967235 && (unsigned int) t[i+1] == 4294967228) {
                result += L"ü";
                i++;
            } else {
                result += t[i];
            }
        }
        results.push_back(result);
    }
    return results;
}

vector<string> splitVector(string strToSplit, char delimeter) {
    stringstream ss(strToSplit);
    string item;
    vector<string> splittedStrings;
    while (getline(ss, item, delimeter))
    {
       splittedStrings.push_back(item);
    }
    return splittedStrings;
}

list<string> split(string str, char del) {
    list<string> results;
    // declaring temp string to store the curr "word" upto del
      string temp = "";

      for(int i=0; i<(int)str.size(); i++){
        // If cur char is not del, then append it to the cur "word", otherwise
        // you have completed the word, print it, and start a new word.
        if(str[i] != del){
            temp += str[i];
        } else {
            bool blank = false;
            if (temp.empty() || all_of(temp.begin(), temp.end(), [](char c){return isspace(c);})) {
                  blank = true;
            }
            string temp2 = temp;
            temp2.erase(remove(temp2.begin(), temp2.end(), ','), temp2.end());
            if (temp2.empty() || all_of(temp2.begin(), temp2.end(), [](char c){return isspace(c);})) {
                  blank = true;
            }
            if (!blank) {
                results.push_back(temp);
            }
            temp = "";
        }
    }
    return results;
}


string read_(tcp::socket & socket) {
    asio::streambuf buf;
    asio::read_until( socket, buf, "\n" );
    auto data = asio::buffer_cast<const char*>(buf.data());
    string s(reinterpret_cast<char const*>(data));
    string result = "";
    return s;
}


void send_(tcp::socket & socket, const string& message) {
    const string msg = message + "\n";
    asio::write( socket, asio::buffer(message) );
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


GLuint CompileShaders(bool vs_b, bool tcs_b, bool tes_b, bool gs_b, bool fs_b) {

	GLuint shader_programme = glCreateProgram();
	GLuint vs, tcs, tes, gs, fs;

	if (vs_b) {
		FILE* vs_file;
		long vs_file_len;
		char* vertex_shader;

		vs_file = fopen("shaders//vs.glsl", "rb");

		fseek(vs_file, 0, SEEK_END);
		vs_file_len = ftell(vs_file);
		rewind(vs_file);

		vertex_shader = (char*)malloc(vs_file_len + 1);


		fread(vertex_shader, vs_file_len, 1, vs_file);
		vertex_shader[vs_file_len] = '\0';
		fclose(vs_file);

		vs = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vs, 1, &vertex_shader, NULL);
		glCompileShader(vs);

		GLint isCompiled = 0;
		glGetShaderiv(vs, GL_COMPILE_STATUS, &isCompiled);

		if (isCompiled == GL_FALSE) {
			GLint maxLength = 0;
			glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			char* error = (char*)malloc(maxLength);
			glGetShaderInfoLog(vs, maxLength, &maxLength, error);
			printf("Vertex shader error: ");
			printf(error);
			free(error);
		}

		glAttachShader(shader_programme, vs);
		free(vertex_shader);
	}
	if (tcs_b) {
		FILE* tcs_file;
		long tcs_file_len;
		char* tessellation_control_shader;

		tcs_file = fopen("shaders//tcs.glsl", "rb");

		fseek(tcs_file, 0, SEEK_END);
		tcs_file_len = ftell(tcs_file);
		rewind(tcs_file);

		tessellation_control_shader = (char*)malloc(tcs_file_len + 1);

		fread(tessellation_control_shader, tcs_file_len, 1, tcs_file);
		tessellation_control_shader[tcs_file_len] = '\0';
		fclose(tcs_file);


		tcs = glCreateShader(GL_TESS_CONTROL_SHADER);
		glShaderSource(tcs, 1, &tessellation_control_shader, NULL);
		glCompileShader(tcs);

		GLint isCompiled = 0;
		glGetShaderiv(tcs, GL_COMPILE_STATUS, &isCompiled);

		if (isCompiled == GL_FALSE) {
			GLint maxLength = 0;
			glGetShaderiv(tcs, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			char* error = (char*)malloc(maxLength);
			glGetShaderInfoLog(tcs, maxLength, &maxLength, error);
			printf("Tessellation control shader error: ");
			printf(error);
			free(error);
		}

		glAttachShader(shader_programme, tcs);
		free(tessellation_control_shader);
	}
	if (tes_b) {
		FILE* tes_file;
		long tes_file_len;
		char* tessellation_evaluation_shader;

		tes_file = fopen("shaders//tes.glsl", "rb");

		fseek(tes_file, 0, SEEK_END);
		tes_file_len = ftell(tes_file);
		rewind(tes_file);

		tessellation_evaluation_shader = (char*)malloc(tes_file_len + 1);

		fread(tessellation_evaluation_shader, tes_file_len, 1, tes_file);
		tessellation_evaluation_shader[tes_file_len] = '\0';
		fclose(tes_file);

		tes = glCreateShader(GL_TESS_EVALUATION_SHADER);
		glShaderSource(tes, 1, &tessellation_evaluation_shader, NULL);
		glCompileShader(tes);

		GLint isCompiled = 0;
		glGetShaderiv(tes, GL_COMPILE_STATUS, &isCompiled);

		if (isCompiled == GL_FALSE) {
			GLint maxLength = 0;
			glGetShaderiv(tes, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			char* error = (char*)malloc(maxLength);
			glGetShaderInfoLog(tes, maxLength, &maxLength, error);
			printf("Tessellation evaluation shader error: ");
			printf(error);
			free(error);
		}

		glAttachShader(shader_programme, tes);
		free(tessellation_evaluation_shader);
	}
	if (gs_b) {
		FILE* gs_file;
		long gs_file_len;
		char* geometry_shader;

		gs_file = fopen("shaders//gs.glsl", "rb");

		fseek(gs_file, 0, SEEK_END);
		gs_file_len = ftell(gs_file);
		rewind(gs_file);

		geometry_shader = (char*)malloc(gs_file_len + 1);

		fread(geometry_shader, gs_file_len, 1, gs_file);
		geometry_shader[gs_file_len] = '\0';
		fclose(gs_file);

		gs = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(gs, 1, &geometry_shader, NULL);
		glCompileShader(gs);

		GLint isCompiled = 0;
		glGetShaderiv(gs, GL_COMPILE_STATUS, &isCompiled);

		if (isCompiled == GL_FALSE) {
			GLint maxLength = 0;
			glGetShaderiv(gs, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			char* error = (char*)malloc(maxLength);
			glGetShaderInfoLog(gs, maxLength, &maxLength, error);
			printf("Geometry shader error: ");
			printf(error);
			free(error);
		}

		glAttachShader(shader_programme, gs);
		free(geometry_shader);
	}
	if (fs_b) {
		FILE* fs_file;
		long fs_file_len;
		char* fragment_shader;

		fs_file = fopen("shaders//fs.glsl", "rb");

		fseek(fs_file, 0, SEEK_END);
		fs_file_len = ftell(fs_file);
		rewind(fs_file);

		fragment_shader = (char*)malloc(fs_file_len + 1);

		fread(fragment_shader, fs_file_len, 1, fs_file);
		fragment_shader[fs_file_len] = '\0';
		fclose(fs_file);

		fs = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fs, 1, &fragment_shader, NULL);
		glCompileShader(fs);

		GLint isCompiled = 0;
		glGetShaderiv(fs, GL_COMPILE_STATUS, &isCompiled);

		if (isCompiled == GL_FALSE) {
			GLint maxLength = 0;
			glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			char* error = (char*)malloc(maxLength);
			glGetShaderInfoLog(fs, maxLength, &maxLength, error);
			printf("Fragment shader error: ");
			printf(error);
			free(error);
		}
		glAttachShader(shader_programme, fs);
		free(fragment_shader);
	}

	glLinkProgram(shader_programme);

	if (vs_b) {
		glDeleteShader(vs);
	}
	if (tcs_b) {
		glDeleteShader(tcs);
	}
	if (tes_b) {
		glDeleteShader(tes);
	}
	if (gs_b) {
		glDeleteShader(gs);
	}
	if (fs_b) {
		glDeleteShader(fs);
	}

	return shader_programme;
}


void monitor_callback(GLFWmonitor* monitor, int event) {
    monitor_event_mutex.lock();
    if (event == GLFW_DISCONNECTED || event == GLFW_CONNECTED) {
        GLFWmonitor** monitors = glfwGetMonitors(&TOTAL_MONITORS);
        Monitor choiceMonitors[TOTAL_MONITORS] = {};
        getMonitors(monitors, choiceMonitors);
        CHOICE_MONITORS = choiceMonitors;
        DEFAULT_MONITOR = CHOICE_MONITORS[0];
        MONITOR_TO_CHANGE = 0;
        glfwSetWindowMonitor(WINDOW, DEFAULT_MONITOR.monitor, 0, 0, DEFAULT_MONITOR.maxResolution.width, DEFAULT_MONITOR.maxResolution.height, DEFAULT_MONITOR.refreshRate);
        monitor_event = true;
    }
    monitor_event_mutex.unlock();
}


void setupMonitor() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_AUTO_ICONIFY, GL_FALSE);
    const GLFWvidmode* mode = glfwGetVideoMode(DEFAULT_MONITOR.monitor);
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    WINDOW = glfwCreateWindow(DEFAULT_MONITOR.maxResolution.width, DEFAULT_MONITOR.maxResolution.height, "OpenTextProjector", DEFAULT_MONITOR.monitor, nullptr);
	glfwMakeContextCurrent(WINDOW);
	glSetup();
}


void glSetup() {
    glewInit();
    //glDebugMessageCallback(GLDebugMessageCallback, NULL);
    glEnable(GL_CULL_FACE);
	glEnable(GL_DEBUG_OUTPUT);
    glViewport(0, 0, DEFAULT_MONITOR.maxResolution.width, DEFAULT_MONITOR.maxResolution.height);
    GLuint shader = CompileShaders(true, false, false, false, true);
	glUseProgram(shader);


	initializeFreeType();

	glm::mat4 projection = glm::ortho(0.0f, (float)DEFAULT_MONITOR.maxResolution.width, 0.0f, (float)DEFAULT_MONITOR.maxResolution.height);
	glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(projection));

	GLuint vao;
	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glCreateBuffers(1, &buffer);

	glNamedBufferStorage(buffer, sizeof(GLfloat) * 6 * 4, NULL, GL_DYNAMIC_STORAGE_BIT);
	glVertexArrayVertexBuffer(vao, 0, buffer, 0, sizeof(GLfloat) * 4);
	glVertexArrayAttribFormat(vao, 0, 4, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(vao, 0, 0);
	glEnableVertexArrayAttrib(vao, 0);
	// Set the text color to white
	glUniform3f(6, RED, GREEN, BLUE);
}


void initializeFreeType() {
	int error;

	FT_Library ft;
	error = FT_Init_FreeType(&ft);

	if (error) {
        printf("Error while trying to initialize freetype library: %d\n", error);
	}

	FT_Face face;
	error = FT_New_Face(ft, FONT, 0, &face);

	if ( error == FT_Err_Unknown_File_Format ) {
      printf("Error: File format not supported\n");
      exit(1);
    }
    else if ( error ) {
      printf("Error while trying to initialize face: %d\n", error);
    }
	FT_Set_Pixel_Sizes(face, 0, FONT_SIZE);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	if(!Characters.empty()) {
        Characters.clear();
	}

	for (GLuint c = 0; c < 512; c++) {
        FT_Load_Char(face, (wchar_t)c, FT_LOAD_RENDER);

		GLuint texture = 0;
		glCreateTextures(GL_TEXTURE_2D,1, &texture);

		glTextureStorage2D(texture, 1, GL_R8, face->glyph->bitmap.width, face->glyph->bitmap.rows);
		glTextureSubImage2D(texture, 0, 0, 0, face->glyph->bitmap.width, face->glyph->bitmap.rows, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);

		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);

		Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<GLuint>(face->glyph->advance.x)
		};
		Characters.insert(pair<wchar_t, Character>((wchar_t) c, character));
	}

	FT_Done_Face(face);
	FT_Done_FreeType(ft);
}


void centerText(int rows, int row) {
    position_mutex.lock();
    TEXT_X = (float)DEFAULT_MONITOR.maxResolution.width/2.0f - ((float)TEXT_WIDTH/2.0f);
    TEXT_Y = (float)DEFAULT_MONITOR.maxResolution.height/2.0f + (((float)rows * (float)TEXT_HEIGHT)/2.0f) - ((float)row * (float)TEXT_HEIGHT);
    position_mutex.unlock();
}


void log(string message) {
    ofstream myfile;
	if (rewriteLogFile) {
        myfile.open("debug.log");
        rewriteLogFile = false;
	} else {
        myfile.open("debug.log", ios_base::app);
	}

	myfile << message << "\n";
	myfile.close();
}


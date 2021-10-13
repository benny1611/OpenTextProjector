#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES

#include <GL/glew.h>
#define GLFW_DLL
#include<GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <map>
#include <iterator>
#include <fstream>
#include <asio.hpp>
#include "main.h"
#include "libraries/tinythread.h"
#include "libraries/json.hpp"

using std::map;
using asio::ip::tcp;
using json = nlohmann::json;
using namespace std;

GLuint CompileShaders(bool vs_b, bool tcs_b, bool tes_b, bool gs_b, bool fs_b);
void getMonitors(GLFWmonitor** monitors, int totalMonitor, Monitor* choiceMonitors);
string read_(tcp::socket & socket);
list<wstring> getTextFromCommand(vector<string> text_list);
list<string> split(string str, char del);
void monitor_callback(GLFWmonitor* monitor, int event);
void setupMonitor();
void glSetup();
void centerText(int rows, int row);
void listen_for_connection(void* aArg);
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
std::list<std::wstring> TEXT;
int FONT_SIZE;
Monitor *CHOICE_MONITORS;
char* FONT;
GLFWwindow* WINDOW;
tthread::mutex monitor_event_mutex;
bool monitor_event = false;
tthread::mutex text_mutex;
tthread::mutex position_mutex;
std::map<wchar_t, Character> Characters;
GLuint buffer;
bool rewriteLogFile = true;


int main(int argc, char *argv[]) {
    tthread::thread t(listen_for_connection, 0);
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    // Look for available monitors
    int totalMonitor;
    GLFWmonitor** monitors = glfwGetMonitors(&totalMonitor);
    Monitor choiceMonitors[totalMonitor] = {};
    getMonitors(monitors, totalMonitor, choiceMonitors);

    for (int i=0; i<totalMonitor; i++) {
        printf("\nMonitor %d found. Name: %s; Resolution: %d X %d\n", i, choiceMonitors[i].name, choiceMonitors[i].maxResolution.height, choiceMonitors[i].maxResolution.width);
    }

    glfwSetMonitorCallback(monitor_callback);

    CHOICE_MONITORS = choiceMonitors;
    DEFAULT_MONITOR = CHOICE_MONITORS[0];
    TEXT_SCALE = 1.0f;
    TEXT.push_back(L"HELLO WORLD! number 1");
    TEXT.push_back(L"HELLO WORLD! x2");
    TEXT_WIDTH = 0;
    TEXT_HEIGHT = 0;
    FONT_SIZE = 30;
    FONT = "./fonts/Raleway-Regular.ttf";
    PADDING = 5;

    printf("Done!\n");

    setupMonitor();

    std::list<int> width_list;
	while (!glfwWindowShouldClose(WINDOW)) {
        monitor_event_mutex.lock();
        if (monitor_event) {
            monitor_event = false;
        }
        monitor_event_mutex.unlock();

        text_mutex.lock();
        GLfloat scale = TEXT_SCALE;
        GLuint globalHeight = 0;
        std::list<std::wstring>::iterator t;
        width_list.clear();
        for (t = TEXT.begin(); t != TEXT.end(); ++t) {
            TEXT_WIDTH = 0;
            TEXT_HEIGHT = 0;
            std::wstring::iterator c;
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
                GLfloat w = ch.Size.x * scale;
                GLfloat h = ch.Size.y * scale;

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
            std::list<int>::iterator it = width_list.begin();
            advance(it, row-1);
            TEXT_WIDTH = *it;
            centerText(TEXT.size(), row);
            row++;
            x = TEXT_X;
            y = TEXT_Y;
            std::wstring::iterator c;
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
                GLfloat w = ch.Size.x * scale;
                GLfloat h = ch.Size.y * scale;

                GLfloat xpos = x + ch.Bearing.x * scale;
                GLfloat ypos = y - (ch.Size.y - ch.Bearing.y) * scale;


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
                x += (ch.Advance >> 6) * scale;
            }
            text_mutex.unlock();
        }
        glfwSwapBuffers(WINDOW);
        glfwPollEvents();
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
                // JSON command
                std::string message = read_(socket);
                json command = json::parse(message);
                // get the text to be shown
                try {
                    std::vector<std::string> text_list = command["text"].get<std::vector<std::string>>();
                    std::list<std::wstring> result = getTextFromCommand(text_list);
                    text_mutex.lock();
                    TEXT.clear();
                    for (std::wstring s: result) {
                        TEXT.push_back(s);
                    }
                    text_mutex.unlock();
                } catch (std::exception& e) {
                    std::cout << e.what() << std::endl;
                }
                // get the text size
                try {
                    int font_size = command["font_size"].get<int>();
                    text_mutex.lock();
                    FONT_SIZE = font_size;
                    text_mutex.unlock();
                } catch (std::exception& e) {
                    //ignore
                }
                //get the font
                try {
                    std::string font = command["font"].get<std::string>();
                    std::ifstream ifile;
                    ifile.open("./fonts/" + font);
                    if (ifile) { // only change font if the font exists
                        text_mutex.lock();
                        ifile.close();
                        char c[font.size() + 1];
                        font.copy(c, font.size() + 1);
                        c[font.size()] = '\0';
                        FONT = c;
                        glSetup();
                        text_mutex.unlock();
                    }
                } catch (std::exception& e) {
                    //ignore
                }
                try {
                    int monitor = command["monitor"].get<int>();
                    int nr_of_monitors = (sizeof CHOICE_MONITORS / sizeof CHOICE_MONITORS[0]);
                    if (monitor < nr_of_monitors && monitor >= 0) {
                        DEFAULT_MONITOR = CHOICE_MONITORS[monitor];
                    }
                } catch (std::exception& e) {
                    //ignore
                }
            }
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    }
}


std::list<std::wstring> getTextFromCommand(std::vector<std::string> text_list) {
    std::list<std::wstring> results;
    std::list<std::string> texts(text_list.begin(), text_list.end());
    for (std::string t: texts) {
        std::wstring result = L"";
        for(int i=0; i<t.length(); i++) {
            if ((unsigned int) t[i] == 92) {
                std::string character = t.substr(i+1, 5);
                i = i+5;
                if (character == "u00e2") {
                    result += L"â";
                } else if (character == "u00c2") {
                    result += L"Â";
                } else if (character == "u0103") {
                    result += L"ă";
                } else if (character == "u0102") {
                    result += L"Ă";
                } else if (character == "u021b") {
                    result += L"ț";
                } else if (character == "u021a") {
                    result += L"Ț";
                } else if (character == "u0219") {
                    result += L"ș";
                } else if (character == "u0218") {
                    result += L"Ș";
                } else if (character == "u00ce") {
                    result += L"Î";
                } else if (character == "u00ee") {
                    result += L"î";
                } else if (character == "u00d6") {
                    result += L"Ö";
                } else if (character == "u00f6") {
                    result += L"ö";
                } else if (character == "u00c4") {
                    result += L"Ä";
                } else if (character == "u00e4") {
                    result += L"ä";
                } else if (character == "u00dc") {
                    result += L"Ü";
                } else if (character == "u00fc") {
                    result += L"ü";
                } else {
                    i = i-5;
                    result += t[i];
                }
            } else {
                result += t[i];
            }
        }
        results.push_back(result);
    }
    return results;
}


std::list<std::string> split(std::string str, char del) {
    std::list<std::string> results;
    // declaring temp string to store the curr "word" upto del
      std::string temp = "";

      for(int i=0; i<(int)str.size(); i++){
        // If cur char is not del, then append it to the cur "word", otherwise
        // you have completed the word, print it, and start a new word.
        if(str[i] != del){
            temp += str[i];
        } else {
            bool blank = false;
            if (temp.empty() || std::all_of(temp.begin(), temp.end(), [](char c){return std::isspace(c);})) {
                  blank = true;
            }
            std::string temp2 = temp;
            temp2.erase(remove(temp2.begin(), temp2.end(), ','), temp2.end());
            if (temp2.empty() || std::all_of(temp2.begin(), temp2.end(), [](char c){return std::isspace(c);})) {
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


std::string read_(tcp::socket & socket) {
    asio::streambuf buf;
    asio::read_until( socket, buf, "\n" );
    auto data = asio::buffer_cast<const char*>(buf.data());
    std::string s(reinterpret_cast<char const*>(data));
    std::string result = "";
    return s;
}


void send_(tcp::socket & socket, const std::string& message) {
    const std::string msg = message + "\n";
    asio::write( socket, asio::buffer(message) );
}


void getMonitors(GLFWmonitor** monitors, int totalMonitor, Monitor* choiceMonitors) {
    printf("\nDetecting monitors ...\n\n");
    map<GLFWmonitor*, GLFWvidmode> monitorsModeMap;

    for(int currMonitor=0;currMonitor<totalMonitor;currMonitor++)
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
        int totalMonitor;
        GLFWmonitor** monitors = glfwGetMonitors(&totalMonitor);
        Monitor choiceMonitors[totalMonitor] = {};
        getMonitors(monitors, totalMonitor, choiceMonitors);
        CHOICE_MONITORS = choiceMonitors;
        DEFAULT_MONITOR = CHOICE_MONITORS[0];
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
    glDebugMessageCallback(GLDebugMessageCallback, NULL);
    glEnable(GL_CULL_FACE);
	glEnable(GL_DEBUG_OUTPUT);
    glViewport(0, 0, DEFAULT_MONITOR.maxResolution.width, DEFAULT_MONITOR.maxResolution.height);
    GLuint shader = CompileShaders(true, false, false, false, true);
	glUseProgram(shader);
	int error;

	FT_Library ft;
	error = FT_Init_FreeType(&ft);

	if (error) {
        printf("Error while trying to initialize library: %d\n", error);
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

		GLuint texture;
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
            face->glyph->advance.x
		};
		Characters.insert(std::pair<wchar_t, Character>((wchar_t) c, character));
	}

	FT_Done_Face(face);
	FT_Done_FreeType(ft);

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

	glUniform3f(6, 0.88f, 0.59f, 0.07f);
}


void centerText(int rows, int row) {
    position_mutex.lock();
    TEXT_X = (float)DEFAULT_MONITOR.maxResolution.width/2.0f - ((float)TEXT_WIDTH/2.0f);
    TEXT_Y = (float)DEFAULT_MONITOR.maxResolution.height/2.0f + (((float)rows * (float)TEXT_HEIGHT)/2.0f) - ((float)row * (float)TEXT_HEIGHT);
    position_mutex.unlock();
}


void log(std::string message) {
    std::ofstream myfile;
	if (rewriteLogFile) {
        myfile.open("debug.log");
        rewriteLogFile = false;
	} else {
        myfile.open("debug.log", std::ios_base::app);
	}

	myfile << message << "\n";
	myfile.close();
}


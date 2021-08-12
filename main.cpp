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
#include <boost/thread.hpp>
#include <map>
#include <iterator>
#include "main.h"


#include <fstream>

using std::map;

GLuint CompileShaders(bool vs_b, bool tcs_b, bool tes_b, bool gs_b, bool fs_b);
void getMonitors(GLFWmonitor** monitors, int totalMonitor, Monitor* choiceMonitors);
void monitor_callback(GLFWmonitor* monitor, int event);
void setupMonitor();
void centerText();
void APIENTRY GLDebugMessageCallback(GLenum source, GLenum type, GLuint id,GLenum severity, GLsizei length,const GLchar* msg, const void* data) {
	printf("%d: %s, severity: %d\n",id, msg, severity);
	/*std::ofstream myfile;
	myfile.open("example.txt", std::ios_base::app);
	myfile << "ID: " << id << " message: " << msg << " severity: " << severity << "\n";
	myfile.close();*/
}

Monitor DEFAULT_MONITOR;
GLfloat TEXT_X;
GLfloat TEXT_Y;
GLfloat TEXT_SCALE;
std::wstring TEXT;
int FONT_SIZE;
Monitor *CHOICE_MONITORS;
char* FONT;
GLFWwindow* WINDOW;
boost::mutex monitor_event_mutex;
bool monitor_event = false;
boost::mutex text_mutex;
std::map<wchar_t, Character> Characters;


int main(int argc, char *argv[]) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_AUTO_ICONIFY, GL_FALSE);
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
    TEXT = L"HELLO WORLD!";
    FONT_SIZE = 20;
    centerText();
    FONT = "./fonts/Raleway-Regular.ttf";

    printf("Done!\n");

    setupMonitor();
    printf("1\n");
    glfwMakeContextCurrent(WINDOW);
    printf("2\n");
    glewInit();
	GLuint shader = CompileShaders(true, false, false, false, true);
	printf("3\n");
	glUseProgram(shader);
	printf("4\n");

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

	GLuint buffer;
	glCreateBuffers(1, &buffer);

	glNamedBufferStorage(buffer, sizeof(GLfloat) * 6 * 4, NULL, GL_DYNAMIC_STORAGE_BIT);
	glVertexArrayVertexBuffer(vao, 0, buffer, 0, sizeof(GLfloat) * 4);
	glVertexArrayAttribFormat(vao, 0, 4, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(vao, 0, 0);
	glEnableVertexArrayAttrib(vao, 0);

	glUniform3f(6, 0.88f, 0.59f, 0.07f);

	bool print = false;

	printf("Entering while loop ... \n");

	while (!glfwWindowShouldClose(WINDOW)) {
        monitor_event_mutex.lock();
        if (monitor_event) {
            glfwDestroyWindow(WINDOW);
            setupMonitor();
            monitor_event = false;
            TEXT = L"COME ON MAAAAN!";
        }
        monitor_event_mutex.unlock();

        text_mutex.lock();
        glClear(GL_COLOR_BUFFER_BIT);
        GLfloat x = TEXT_X;
        GLfloat y = TEXT_Y;
        GLfloat scale = TEXT_SCALE;
		std::wstring::iterator c;
		for (c = TEXT.begin(); c != TEXT.end(); c++) {

			Character ch = Characters[*c];
			if (print) {
                printf("Here: %c and the int: %d\n", (wchar_t)*c, *c);
			}
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

			GLfloat xpos = x + ch.Bearing.x * scale;
			GLfloat ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

			GLfloat w = ch.Size.x * scale;
			GLfloat h = ch.Size.y * scale;
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
		print = false;

		glfwSwapBuffers(WINDOW);

		glfwPollEvents();

	}
	printf("%d", glGetError());

    glfwTerminate();
    return 0;
}



void getMonitors(GLFWmonitor** monitors, int totalMonitor, Monitor* choiceMonitors) {
    printf("\nDetecting monitors ...\n\n");
    //int totalMonitor = monitors
    map<GLFWmonitor*, GLFWvidmode> monitorsModeMap;
    //Monitor choiceMonitors[totalMonitor];

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
        i++;
    }
}


GLuint CompileShaders(bool vs_b, bool tcs_b, bool tes_b, bool gs_b, bool fs_b) {

    printf("Kill me please\n");

	GLuint shader_programme = glCreateProgram();

	printf("Kill me please2\n");

	GLuint vs, tcs, tes, gs, fs;

	if (vs_b) {
        printf("In vs_b\n");
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
		printf("Done with vs..\n");
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
        printf("In fs_b ...\n");
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
		printf("Done with fs...\n");
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
        monitor_event = true;
    }
    monitor_event_mutex.unlock();
}

void setupMonitor() {

    const GLFWvidmode* mode = glfwGetVideoMode(DEFAULT_MONITOR.monitor);

    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    WINDOW = glfwCreateWindow(DEFAULT_MONITOR.maxResolution.width, DEFAULT_MONITOR.maxResolution.height, "OpenTextProjector", NULL, NULL);
    //glfwMakeContextCurrent(WINDOW);
    glEnable(GL_CULL_FACE);
	glEnable(GL_DEBUG_OUTPUT);
	//glDebugMessageCallback(GLDebugMessageCallback, NULL);
	glfwSetWindowMonitor(WINDOW, DEFAULT_MONITOR.monitor, 0, 0, DEFAULT_MONITOR.maxResolution.width, DEFAULT_MONITOR.maxResolution.height, mode->refreshRate);
	glViewport(0, 0, DEFAULT_MONITOR.maxResolution.width, DEFAULT_MONITOR.maxResolution.height);
	centerText();
}

void centerText() {
    TEXT_X = (float)DEFAULT_MONITOR.maxResolution.width/2.0f - (((float)TEXT.length()*FONT_SIZE)/2.0f);
    TEXT_Y = (float)DEFAULT_MONITOR.maxResolution.height/2.0f - ((float)FONT_SIZE/2.0f);
}


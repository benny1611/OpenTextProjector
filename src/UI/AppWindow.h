#pragma once

#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <Poco/AutoPtr.h>
#include <Poco/Util/PropertyFileConfiguration.h>

// ImGui Includes
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl2.h>

class AppWindow {
public:
    AppWindow(const std::string& title, const std::string& url, const Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> config, const std::string& configFilePath);
    ~AppWindow();

    bool init();
    void draw();
    bool shouldClose() const;
    GLFWwindow* getWindow();

private:
    std::string m_title;
    std::string m_url;
    std::string m_configFilePath;
    Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> m_config;

    GLFWwindow* m_window;
    GLuint m_qrTexture;
    ImFont* m_titleFont;
    ImFont* m_paragraphFont;
    ImFont* m_buttonFont;

    // UI State
    bool m_neverShowAgain;
    bool m_closeRequested;

    // Helper methods
    GLuint generateQrTexture(const char* text, int scale = 10);
    void updatePropertiesFile();
};
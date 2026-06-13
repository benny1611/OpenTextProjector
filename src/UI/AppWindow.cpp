#include "AppWindow.h"
#include "../Util/qrcodegen.hpp"
#include <iostream>
#include <fstream>
#include <vector>

using qrcodegen::QrCode;

AppWindow::AppWindow(const std::string& title, const std::string& url, const Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> config, const std::string& configFilePath)
    : m_title(title), m_url(url), m_config(config), m_configFilePath(configFilePath),
    m_window(nullptr), m_qrTexture(0), m_neverShowAgain(false), m_closeRequested(false) {
}

AppWindow::~AppWindow() {
    if (!m_destroyed) {
        destroy();
    }
}

void AppWindow::destroy() {
    //Make this window's context current before deleting OpenGL resources!
    if (m_window) {
        glfwMakeContextCurrent(m_window);
    }

    //ImGui Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    //Delete custom textures
    if (m_qrTexture != 0) {
        glDeleteTextures(1, &m_qrTexture);
        m_qrTexture = 0; // Good practice to zero out
    }

    //Destroy the GLFW window
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    m_destroyed = true;
}

bool AppWindow::init(MonitorInfo monitorInfo) {
    m_window = glfwCreateWindow(800, 600, m_title.c_str(), nullptr, nullptr);
    if (!m_window) return false;

    glfwMakeContextCurrent(m_window);

    glfwSetWindowPos(m_window, monitorInfo.xpos + 100, monitorInfo.ypos + 100);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return false;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL2_Init();

    m_titleFont = io.Fonts->AddFontFromFileTTF("./resources/fonts/Raleway.ttf", 32.0f);
    m_paragraphFont = io.Fonts->AddFontFromFileTTF("./resources/fonts/Raleway.ttf", 24.0f);
    m_buttonFont = io.Fonts->AddFontFromFileTTF("./resources/fonts/Raleway.ttf", 16.0f);

    ImGui_ImplOpenGL2_CreateFontsTexture();

    m_qrTexture = generateQrTexture(m_url.c_str());
    return true;
}

GLuint AppWindow::generateQrTexture(const char* text, int scale) {
    GLuint qrTexture = 0;
    QrCode qr = QrCode::encodeText(text, QrCode::Ecc::MEDIUM);
    int size = qr.getSize();
    int imgSize = size * scale;

    // Create RGBA image (White background, Black QR)
    std::vector<uint8_t> image(imgSize * imgSize * 4, 255); // White background

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            bool black = qr.getModule(x, y);
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    int px = (y * scale + dy) * imgSize + (x * scale + dx);
                    int index = px * 4;
                    if (black) {
                        image[index] = image[index + 1] = image[index + 2] = 0; // Black
                    }
                    image[index + 3] = 255; // Alpha
                }
            }
        }
    }

    // Upload to OpenGL texture
    if (qrTexture) {
        glDeleteTextures(1, &qrTexture);
    }
    glGenTextures(1, &qrTexture);
    glBindTexture(GL_TEXTURE_2D, qrTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgSize, imgSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    return qrTexture;
}

void AppWindow::draw() {
    if (glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) != 0) {
        ImGui_ImplGlfw_Sleep(10);
        return;
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height); // Get current GLFW window size

    ImGui::SetNextWindowPos(ImVec2(0, 0)); // Position at top-left
    ImGui::SetNextWindowSize(ImVec2(width, height)); // Match GLFW size

    ImGui::Begin("Simple Text Projector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    ImGui::PushFont(m_titleFont);


    const char* title = "Welcome to Simple Text Projector";
    ImVec2 titleSize = ImGui::CalcTextSize(title);

    float windowWidth = ImGui::GetWindowSize().x;
    float titleX = (windowWidth - titleSize.x) * 0.5f;

    ImGui::SetCursorPosX(titleX);
    ImGui::Text("%s", title);
    ImGui::PopFont();


    ImGui::PushFont(m_paragraphFont);

    float windowHeight = ImGui::GetWindowSize().y;
    float spacerHeight = windowHeight / 8.0f;

    // Add vertical space
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spacerHeight);

    std::string linkP = "You can access the interface by accessing this link: ";

    // Calculate individual text sizes
    ImVec2 linkPSize = ImGui::CalcTextSize(linkP.c_str());
    ImVec2 urlSize = ImGui::CalcTextSize(m_url.c_str());

    float totalTextWidth = linkPSize.x + urlSize.x;

    // Center both texts together
    float linkPX = (windowWidth - totalTextWidth) * 0.5f;
    ImGui::SetCursorPosX(linkPX);

    // Print the normal text
    ImGui::Text("%s", linkP.c_str());
    ImGui::SameLine();

    ImVec4 linkColor = ImVec4(0.1f, 0.5f, 1.0f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, linkColor);
    ImGui::Text("%s", m_url.c_str());
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    if (ImGui::IsItemClicked()) {
#ifdef _WIN32
        system(("start " + m_url).c_str());
#elif __APPLE__
        system(("open " + m_url).c_str());
#else
        system(("xdg-open " + m_url).c_str());
#endif
    }


    ImGui::NewLine();

    std::string qrCodeP = "Or scan this QR code:";

    ImVec2 qrCodePSize = ImGui::CalcTextSize(qrCodeP.c_str());

    float qrCodePX = (windowWidth - qrCodePSize.x) * 0.5f;

    ImGui::SetCursorPosX(qrCodePX);
    ImGui::Text("%s", qrCodeP.c_str());

    ImVec2 availableSpace = ImGui::GetContentRegionAvail();

    float qrXOffset = (availableSpace.x - 256) * 0.5f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + qrXOffset);

    ImGui::Image(m_qrTexture, ImVec2(256, 256));


    ImGui::NewLine();
    std::string checkBoxText = "Don't show this window on startup anymore ";
    ImVec2 checkBoxTextSize = ImGui::CalcTextSize(checkBoxText.c_str());
    float checkBoxTextX = (windowWidth - checkBoxTextSize.x) * 0.5;
    ImGui::SetCursorPosX(checkBoxTextX);
    ImGui::Text(checkBoxText.c_str());
    ImGui::SameLine();
    ImGui::Checkbox("##", &m_neverShowAgain);

    ImGui::PopFont();

    ImGui::PushFont(m_buttonFont);

    float buttonHeight = 30.0f;
    float spacing = 10.0f;
    float availableHeight = ImGui::GetContentRegionAvail().y;

    // Move cursor to bottom area for button placement
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + availableHeight - buttonHeight - spacing);

    // Center the button horizontally
    float buttonWidth = 100.0f; // Adjust as needed
    float buttonX = (windowWidth - buttonWidth) * 0.5f;
    ImGui::SetCursorPosX(buttonX);

    if (ImGui::Button("Close", ImVec2(buttonWidth, buttonHeight))) {
        m_closeRequested = true;
    }

    ImGui::PopFont();
    ImGui::End();

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

bool AppWindow::shouldClose() const {
    bool osClose = glfwWindowShouldClose(m_window);

    // If the OS 'X' was clicked OR the internal ImGui button was clicked
    if (osClose || m_closeRequested) {
        // Safe check to avoid rewriting multiple times if called in a loop
        if (m_neverShowAgain) {
            const_cast<AppWindow*>(this)->updatePropertiesFile();
        }
        return true;
    }
    return false;
}

void AppWindow::updatePropertiesFile() {
    m_config->setBool("app.showhelp", false);
    m_config->save(m_configFilePath);
}

GLFWwindow* AppWindow::getWindow() {
    return m_window;
}
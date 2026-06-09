#include "MonitorManager.h"
#include <iostream>

// GLFW uses a C-style callback, so we need a bridge to route it to our instance.
static MonitorManager* g_MonitorManagerInstance = nullptr;

void glfw_monitor_callback(GLFWmonitor* monitor, int event) {
    if (g_MonitorManagerInstance) {
        g_MonitorManagerInstance->onMonitorEvent(monitor, event);
    }
}

MonitorManager::MonitorManager() : activeMonitorIndex(-1) {
    g_MonitorManagerInstance = this;
}

MonitorManager::~MonitorManager() {
    g_MonitorManagerInstance = nullptr;
}

void MonitorManager::init() {
    glfwSetMonitorCallback(glfw_monitor_callback);
    refreshMonitors();
}

void MonitorManager::refreshMonitors() {
    std::unique_lock lock(mtx); // Write lock

    monitors.clear();
    int count;
    GLFWmonitor** glfwMonitors = glfwGetMonitors(&count);
    GLFWmonitor* primary = glfwGetPrimaryMonitor();

    for (int i = 0; i < count; i++) {
        MonitorInfo info;
        info.handle = glfwMonitors[i];
        info.name = glfwGetMonitorName(glfwMonitors[i]);

        const GLFWvidmode* mode = glfwGetVideoMode(glfwMonitors[i]);
        if (mode) {
            info.width = mode->width;
            info.height = mode->height;
            info.refreshRate = mode->refreshRate;
        }

        glfwGetMonitorPos(glfwMonitors[i], &info.xpos, &info.ypos);
        monitors.push_back(info);

        if (activeMonitorIndex == -1) { // first time start, set the primary as active
            if (glfwMonitors[i] == primary) {
                activeMonitorIndex = i;
            }
        }
    }

    // Reset active monitor to primary if the previous active one was unplugged
    if (activeMonitorIndex >= monitors.size()) {
        activeMonitorIndex = 0;
    }
}

void MonitorManager::onMonitorEvent(GLFWmonitor* monitor, int event) {
    if (event == GLFW_CONNECTED || event == GLFW_DISCONNECTED) {
        // A monitor was plugged or unplugged. Rebuild the list.
        // NOTE: This callback is fired ON THE MAIN THREAD during glfwPollEvents().
        refreshMonitors();
    }
}

std::vector<MonitorInfo> MonitorManager::getMonitors() const {
    std::shared_lock lock(mtx);
    return monitors;
}

MonitorInfo MonitorManager::getActiveMonitor() const {
    std::shared_lock lock(mtx);
    if (monitors.empty()) return MonitorInfo{};
    return monitors[activeMonitorIndex];
}

bool MonitorManager::setActiveMonitor(const std::string& monitorName) {
    std::unique_lock lock(mtx);
    for (size_t i = 0; i < monitors.size(); ++i) {
        if (monitors[i].name == monitorName) {
            activeMonitorIndex = i;
            return true;
        }
    }
    return false; // Monitor not found
}
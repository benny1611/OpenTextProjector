#pragma once
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <shared_mutex>
#include <memory>

struct MonitorInfo {
    std::string name;
    int width = 0;
    int height = 0;
    int xpos = 0;
    int ypos = 0;
    int refreshRate = GLFW_DONT_CARE;
    GLFWmonitor* handle = nullptr;

    bool operator==(const MonitorInfo& other) const
    {
        return handle == other.handle;
    }

    bool operator!=(const MonitorInfo& other) const
    {
        return handle != other.handle;
    }
};

class MonitorManager {
public:
    MonitorManager();
    ~MonitorManager();

    // Initializes the monitor list (Call this once after glfwInit)
    void init();

    // Thread-safe getters
    std::vector<MonitorInfo> getMonitors() const;
    MonitorInfo getActiveMonitor() const;

    // Thread-safe setters
    bool setActiveMonitor(const std::string& monitorName);

    // Callback used by GLFW
    void onMonitorEvent(GLFWmonitor* monitor, int event);

private:
    void refreshMonitors();

    mutable std::shared_mutex mtx;
    std::vector<MonitorInfo> monitors;
    int activeMonitorIndex;
};
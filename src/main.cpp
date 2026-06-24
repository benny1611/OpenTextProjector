#include "Server/AuthServer.h"
#include "Core/DIContainer.h"
#include "Core/AppConfig.h"
#include "Repositories/UserRepository.h"
#include "Services/AuthService.h"
#include "Util/ThreadSafeQueue.h"
#include "Models/Command.h"
#include "Services/CommandProcessor.h"
#include "Controllers/WebSocketController.h"
#include "UI/AppWindow.h"
#include "System/MonitorManager.h"
#include "System/TextManager.h"

#include <Poco/Data/Session.h>
#include <Poco/Data/SessionPool.h>
#include <Poco/Data/SQLite/Connector.h>
#include <Poco/Util/PropertyFileConfiguration.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/Net/NetworkInterface.h>
#include <Poco/AutoPtr.h>
#include <Poco/File.h>
#include <Poco/DirectoryIterator.h>
#include <Poco/FileStream.h>
#include <Poco/StreamCopier.h>
#include <Poco/Exception.h>
#include <Poco/FormattingChannel.h>
#include <Poco/PatternFormatter.h>
#include <Poco/Logger.h>
#include <Poco/ConsoleChannel.h>
#include <Poco/FileChannel.h>
#include <Poco/Message.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <ft2build.h>
#include <freetype/freetype.h>

#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <vector>
#include <algorithm>

std::atomic<bool> g_appRunning{true};

static void glfw_error_callback(int error, const char* description);

void handleSignal(int) {
    g_appRunning = false;
}

// Helper function to run hard-coded database migrations
void runMigrations(std::shared_ptr<Poco::Data::SessionPool> pool, Poco::Logger& logger) {
    // 1. Define your migrations in the order they should execute
    const std::vector<std::string> migrations = {
        // Migration 1: 01_init_users
        R"(
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username VARCHAR(50) UNIQUE NOT NULL,
                password_hash VARCHAR(255) NOT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
        )"
        
        // When you need a second migration, just add a comma and drop it here!
        /*
        , R"(
            CREATE TABLE IF NOT EXISTS some_new_table ( ... );
        )"
        */
    };

    // 2. Execute them safely
    try {
        Poco::Data::Session session(pool->get());
        int step = 1;

        for (const auto& query : migrations) {
            // Poco::Data::Keywords::now forces the query to execute immediately
            session << query, Poco::Data::Keywords::now;
            logger.information("Executed Migration Step #" + std::to_string(step));
            step++;
        }
    } 
    catch (const Poco::Exception& ex) {
        logger.fatal("Migration failed: " + ex.displayText());
    }
}

void initializeLogging() {
    Poco::AutoPtr<Poco::PatternFormatter> pFormatter(
        new Poco::PatternFormatter("%Y-%m-%d %H:%M:%S.%i [%p] %s: %t")
    );
    Poco::AutoPtr<Poco::FormattingChannel> pFC(new Poco::FormattingChannel(pFormatter));

#ifndef NDEBUG
    Poco::AutoPtr<Poco::ColorConsoleChannel> pConsoleChannel(new Poco::ColorConsoleChannel);
    pConsoleChannel->setProperty("traceColor", "darkGray");
    pConsoleChannel->setProperty("debugColor", "gray");
    pConsoleChannel->setProperty("informationColor", "white");
    pConsoleChannel->setProperty("warningColor", "yellow");
    pConsoleChannel->setProperty("errorColor", "red");
    pConsoleChannel->setProperty("fatalColor", "cyan");
    pFC->setChannel(pConsoleChannel);
    Poco::Logger::root().setLevel(Poco::Message::PRIO_DEBUG);
#else
    Poco::AutoPtr<Poco::FileChannel> pFileChannel(new Poco::FileChannel("log.txt"));
    pFileChannel->setProperty("rotation", "5 M");
    pFileChannel->setProperty("archive", "timestamp");
    pFC->setChannel(pFileChannel);
    Poco::Logger::root().setLevel(Poco::Message::PRIO_INFORMATION);
#endif

    pFC->open();

    // Attach this setup to the ROOT logger
    Poco::Logger::root().setChannel(pFC);
}

int main() {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    initializeLogging();
    Poco::Logger& logger = Poco::Logger::get("main");

    // Setup Environment
    Poco::Net::initializeSSL();
    Poco::Data::SQLite::Connector::registerConnector();
    
    Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> config;
    try {
        config = new Poco::Util::PropertyFileConfiguration("OpenTextProjector.properties");
    } catch (...) {
        logger.error("OpenTextProjector.properties not found!");
        return 1;
    }

    // Setup DI Container
    auto container = std::make_shared<DIContainer>();

    // Register Database & Config
    auto dbPool = std::make_shared<Poco::Data::SessionPool>("SQLite", config->getString("db.connection_string"));
    container->registerService(dbPool);

    auto appConfig = std::make_shared<AppConfig>();
    appConfig->jwtSecret = config->getString("jwt.secret");
    appConfig->jwtExpHours = config->getInt("jwt.expiration_hours");
    container->registerService(appConfig);

    // Register Repositories and Services (Dependency Injection)
    auto userRepo = std::make_shared<UserRepository>(container->resolve<Poco::Data::SessionPool>());
    container->registerService(userRepo);

    auto authService = std::make_shared<AuthService>(
        container->resolve<UserRepository>(), 
        container->resolve<AppConfig>()
    );
    container->registerService(authService);

    // Create and Register the Shared Queue
    auto commandQueue = std::make_shared<ThreadSafeQueue<Command>>();
    container->registerService<ThreadSafeQueue<Command>>(commandQueue);

    // Create, Start, and Register the Worker Thread Service
    auto commandProcessor = std::make_shared<CommandProcessor>(commandQueue, container);
    commandProcessor->start();
    container->registerService<CommandProcessor>(commandProcessor);

    // Run Database Migrations
    runMigrations(dbPool, logger);

    // Get the url to show
    std::string url = "";
    if (config->hasProperty("app.hostname")) {
        url = config->getString("app.hostname", "");
    }
    
    if (url.empty()) {
        Poco::Net::NetworkInterface::Map interfaces = Poco::Net::NetworkInterface::map();
        for (const auto& entry : interfaces) {
            const Poco::Net::NetworkInterface& netInterface = entry.second;
            if (netInterface.supportsIPv4() && !netInterface.isLoopback() && netInterface.isRunning()) {
                url = netInterface.address().toString();
                break;
            }
        }
    }

    bool useHttps = config->getBool("server.use_https", false);
    if (useHttps && !url._Starts_with("https://")) {
        url = "https://" + url;
    }
    else if(!useHttps && !url._Starts_with("http://")) {
        url = "http://" + url;
    }

    logger.information("URL: " + url);

    // Start the Server
    AuthServer apiServer(container, *config);
    apiServer.start();
    logger.information("API Server running. Starting main task loop...");

    // Initialize GLFW
    glfwSetErrorCallback(glfw_error_callback);
    int err = glfwInit();
    if (err != GLFW_TRUE) {
        logger.error("GLFW init failed!");
        exit(1);
    }

    auto monitorManager = std::make_shared<MonitorManager>();
    monitorManager->init();
    container->registerService(monitorManager);

    // Check if we should show the UI
    bool showHelp = config->getBool("app.showhelp", true);
    AppWindow ui("OpenTextProjector Help", url, config, "OpenTextProjector.properties");
    if (showHelp) {
        std::vector<MonitorInfo> monitors = monitorManager->getMonitors();
        MonitorInfo helpUIMonitorInfo = monitorManager->getActiveMonitor();
        // Show the help UI on a separate monitor if possile
        for (const auto& mon : monitors) {
            if (mon != helpUIMonitorInfo) {
                helpUIMonitorInfo = mon;
                break;
            }
        }
        if (!ui.init(helpUIMonitorInfo)) {
            logger.error("Failed to initialize UI!");
            glfwTerminate();
            exit(1);
        }
    }

    // Get the main window ready
    glfwWindowHint(GLFW_AUTO_ICONIFY, GL_FALSE);
    MonitorInfo primary = monitorManager->getActiveMonitor();

    GLFWwindow* window = glfwCreateWindow(primary.width, primary.height, "OpenTextProjector", primary.handle, NULL);

    if (window == NULL) {
        glfwTerminate();
        logger.error("Could not create the GLFW window");
        exit(1);
    }

    // Create the text manager
    auto textManager = std::make_shared<TextManager>();
    container->registerService<TextManager>(textManager);
    
    int id = textManager->createTextBox(200, 200, 200, 200);
    textManager->setText(id, "test!!");
    textManager->setAlignment(id, TextAlignment::Right);
    textManager->setDebugMode(id, true);

    // Main Application Loop
    while (g_appRunning && !glfwWindowShouldClose(window)) {
        MonitorInfo activeMonitor = monitorManager->getActiveMonitor();
        if (activeMonitor.handle != glfwGetWindowMonitor(window)) {
            glfwSetWindowMonitor(window, activeMonitor.handle, 0, 0, activeMonitor.width, activeMonitor.height, activeMonitor.refreshRate);
        }
        glfwPollEvents();
        glfwMakeContextCurrent(window);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        textManager->renderAll();

        glfwSwapBuffers(window);

        if (showHelp) {
            if (!ui.shouldClose()) {
                glfwMakeContextCurrent(ui.getWindow());
                ui.draw();
                glfwSwapBuffers(ui.getWindow());
            }
            else {
                showHelp = false;
                ui.destroy();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Graceful Teardown
    logger.information("Shutting down...");
    apiServer.stop();
    Poco::Data::SQLite::Connector::unregisterConnector();
    Poco::Net::uninitializeSSL();

    return 0;
}


static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}
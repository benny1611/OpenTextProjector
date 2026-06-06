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
#include <Poco/Message.h>
#include <Poco/ConsoleChannel.h>
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
void runMigrations(std::shared_ptr<Poco::Data::SessionPool> pool) {
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
            std::cout << "Executed Migration Step #" << step++ << "\n";
        }
    } 
    catch (const Poco::Exception& ex) {
        // It is highly recommended to catch Poco exceptions here 
        // so you know exactly why a SQL statement failed!
        std::cerr << "Migration failed: " << ex.displayText() << "\n";
    }
}

int main() {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    // set up two channel chains - one to the
    // console and the other one to a log file.
    Poco::FormattingChannel* pFCConsole = new Poco::FormattingChannel(new Poco::PatternFormatter("[%O] %s: %p: %t"));
    pFCConsole->setChannel(new Poco::ConsoleChannel);
    pFCConsole->open();
    Poco::Logger& consoleLogger = Poco::Logger::create("OpenTextProjector.main", pFCConsole, Poco::Message::PRIO_INFORMATION);

    // Setup Environment
    Poco::Net::initializeSSL();
    Poco::Data::SQLite::Connector::registerConnector();
    
    Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> config;
    try {
        config = new Poco::Util::PropertyFileConfiguration("OpenTextProjector.properties");
    } catch (...) {
        consoleLogger.error("OpenTextProjector.properties not found!");
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
    auto commandProcessor = std::make_shared<CommandProcessor>(commandQueue);
    commandProcessor->start();
    container->registerService<CommandProcessor>(commandProcessor);

    // Run Database Migrations
    runMigrations(dbPool);

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

    consoleLogger.information("URL: " + url);

    // Start the Server
    AuthServer apiServer(container, *config);
    apiServer.start();
    consoleLogger.information("API Server running. Starting main task loop...");

    // Initialize GLFW
    glfwSetErrorCallback(glfw_error_callback);
    int err = glfwInit();
    if (err != GLFW_TRUE) {
        consoleLogger.error("GLFW init failed!");
        exit(1);
    }

    // Check if we should show the UI
    bool showHelp = config->getBool("app.showhelp", true);
    AppWindow ui("OpenTextProjector Help", url, config, "OpenTextProjector.properties");
    if (showHelp) {
        // Pass the actual file path so the window can update it later
        if (!ui.init()) {
            consoleLogger.error("Failed to initialize UI!");
            glfwTerminate();
            exit(1);
        }
    }

    // Main Application Loop
    while (g_appRunning) {
        glfwPollEvents();

        if (showHelp) {
            if (!ui.shouldClose()) {
                ui.draw();
                glfwMakeContextCurrent(ui.getWindow());
                glfwSwapBuffers(ui.getWindow());
            } else {
                showHelp = false;
                ui.~AppWindow();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Graceful Teardown
    consoleLogger.information("Shutting down...");
    apiServer.stop();
    Poco::Data::SQLite::Connector::unregisterConnector();
    Poco::Net::uninitializeSSL();

    return 0;
}


static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}
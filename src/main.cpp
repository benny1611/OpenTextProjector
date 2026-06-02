#include "Server/AuthServer.h"
#include "Core/DIContainer.h"
#include "Core/AppConfig.h"
#include "Repositories/UserRepository.h"
#include "Services/AuthService.h"
#include "Util/ThreadSafeQueue.h"
#include "Models/Command.h"
#include "Services/CommandProcessor.h"
#include "Controllers/WebSocketController.h"

#include <Poco/Data/Session.h>
#include <Poco/Data/SessionPool.h>
#include <Poco/Data/SQLite/Connector.h>
#include <Poco/Util/PropertyFileConfiguration.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/AutoPtr.h>
#include <Poco/File.h>
#include <Poco/DirectoryIterator.h>
#include <Poco/FileStream.h>
#include <Poco/StreamCopier.h>
#include <Poco/Exception.h>

#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <vector>
#include <algorithm>

std::atomic<bool> g_appRunning{true};

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

    // 1. Setup Environment
    Poco::Net::initializeSSL();
    Poco::Data::SQLite::Connector::registerConnector();
    
    Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> config;
    try {
        config = new Poco::Util::PropertyFileConfiguration("OpenTextProjector.properties");
    } catch (...) {
        std::cerr << "OpenTextProjector.properties not found!\n";
        return 1;
    }

    // 2. Setup DI Container
    auto container = std::make_shared<DIContainer>();

    // 3. Register Database & Config
    auto dbPool = std::make_shared<Poco::Data::SessionPool>("SQLite", config->getString("db.connection_string"));
    container->registerService(dbPool);

    auto appConfig = std::make_shared<AppConfig>();
    appConfig->jwtSecret = config->getString("jwt.secret");
    appConfig->jwtExpHours = config->getInt("jwt.expiration_hours");
    container->registerService(appConfig);

    // 4. Register Repositories and Services (Dependency Injection)
    auto userRepo = std::make_shared<UserRepository>(container->resolve<Poco::Data::SessionPool>());
    container->registerService(userRepo);

    auto authService = std::make_shared<AuthService>(
        container->resolve<UserRepository>(), 
        container->resolve<AppConfig>()
    );
    container->registerService(authService);

    // 5. Create and Register the Shared Queue
    auto commandQueue = std::make_shared<ThreadSafeQueue<Command>>();
    container->registerService<ThreadSafeQueue<Command>>(commandQueue);

    // 6. Create, Start, and Register the Worker Thread Service
    auto commandProcessor = std::make_shared<CommandProcessor>(commandQueue);
    commandProcessor->start();
    container->registerService<CommandProcessor>(commandProcessor);

    // 5. Run Database Migrations
    runMigrations(dbPool);

    // 6. Start the Server
    AuthServer apiServer(container, *config);
    apiServer.start();
    std::cout << "API Server running. Starting main task loop...\n";

    // 7. Main Application Loop
    while (g_appRunning) {
        // --> YOUR PRIMARY WORKLOAD HERE <--
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 8. Graceful Teardown
    std::cout << "Shutting down...\n";
    apiServer.stop();
    Poco::Data::SQLite::Connector::unregisterConnector();
    Poco::Net::uninitializeSSL();

    return 0;
}
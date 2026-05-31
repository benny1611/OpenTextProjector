#pragma once
#include <Poco/Net/HTTPServer.h>
#include <Poco/Util/PropertyFileConfiguration.h>
#include "../Core/DIContainer.h"
#include <memory>

class AuthServer {
private:
    std::unique_ptr<Poco::Net::HTTPServer> _server;

public:
    AuthServer(std::shared_ptr<DIContainer> container, Poco::Util::PropertyFileConfiguration& config);
    ~AuthServer();
    void start();
    void stop();
};
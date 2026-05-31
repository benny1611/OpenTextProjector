#include "AuthServer.h"
#include "../Router/AuthRouter.h"
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/SecureServerSocket.h>
#include <Poco/Net/Context.h>

AuthServer::AuthServer(std::shared_ptr<DIContainer> container, Poco::Util::PropertyFileConfiguration& config) {
    bool useHttps = config.getBool("server.use_https", false);
    unsigned short port = config.getInt(useHttps ? "server.https_port" : "server.port", 8080);

    std::unique_ptr<Poco::Net::ServerSocket> socket;

    if (useHttps) {
        std::string cert = config.getString("ssl.certificate_file");
        std::string key = config.getString("ssl.private_key_file");
        
        Poco::Net::Context::Ptr pContext = new Poco::Net::Context(
            Poco::Net::Context::SERVER_USE, key, cert, "", 
            Poco::Net::Context::VERIFY_RELAXED
        );
        socket = std::make_unique<Poco::Net::SecureServerSocket>(port, 64, pContext);
    } else {
        socket = std::make_unique<Poco::Net::ServerSocket>(port);
    }

    _server = std::make_unique<Poco::Net::HTTPServer>(
        new AuthRouter(container), 
        *socket, 
        new Poco::Net::HTTPServerParams
    );
}

AuthServer::~AuthServer() { stop(); }
void AuthServer::start() { _server->start(); }
void AuthServer::stop() { _server->stop(); }
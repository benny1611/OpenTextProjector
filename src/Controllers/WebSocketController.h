#pragma once
#include <Poco/Net/HTTPRequestHandler.h>
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "../Services/AuthService.h"
#include "../Util/ThreadSafeQueue.h"
#include "../Models/Command.h"
#include <memory>

class WebSocketController : public Poco::Net::HTTPRequestHandler {
public:
    WebSocketController(
        std::shared_ptr<AuthService> authService,
        std::shared_ptr<ThreadSafeQueue<Command>> commandQueue
    );

    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);

private:
    std::string extractToken(const std::string& uriString);

    std::shared_ptr<AuthService> _authService;
    std::shared_ptr<ThreadSafeQueue<Command>> _commandQueue;
};
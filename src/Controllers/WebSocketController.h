#pragma once
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "../Services/AuthService.h"
#include <memory>
#include <string>

class WebSocketController : public Poco::Net::HTTPRequestHandler {
public:
    explicit WebSocketController(std::shared_ptr<AuthService> authService);

    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

private:
    std::shared_ptr<AuthService> _authService; // Store as shared_ptr
    std::string extractToken(const std::string& uri);
};
#pragma once
#include <Poco/Net/HTTPRequestHandler.h>
#include "../Services/AuthService.h"
#include <memory>

class LoginController : public Poco::Net::HTTPRequestHandler {
private:
    std::shared_ptr<AuthService> _authService;
public:
    explicit LoginController(std::shared_ptr<AuthService> service);
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;
};
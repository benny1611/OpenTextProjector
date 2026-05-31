#pragma once
#include <Poco/Net/HTTPRequestHandler.h>
#include "../Services/AuthService.h"
#include <memory>

class RegisterController : public Poco::Net::HTTPRequestHandler {
private:
    std::shared_ptr<AuthService> _authService;
public:
    explicit RegisterController(std::shared_ptr<AuthService> service);
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;
};
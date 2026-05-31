#pragma once
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include "../Core/DIContainer.h"
#include <memory>

class AuthRouter : public Poco::Net::HTTPRequestHandlerFactory {
private:
    std::shared_ptr<DIContainer> _container;

public:
    explicit AuthRouter(std::shared_ptr<DIContainer> container);
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};
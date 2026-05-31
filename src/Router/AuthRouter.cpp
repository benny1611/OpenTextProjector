#include "AuthRouter.h"
#include "../Controllers/RegisterController.h"
#include "../Controllers/LoginController.h"
#include "../Services/AuthService.h"
#include <Poco/Net/HTTPServerRequest.h>

AuthRouter::AuthRouter(std::shared_ptr<DIContainer> container) : _container(container) {}

// POCO takes ownership of the returned raw pointer and deletes it when the request is done.
// We safely pass in our Singleton Services via the DI container.
Poco::Net::HTTPRequestHandler* AuthRouter::createRequestHandler(const Poco::Net::HTTPServerRequest& request) {
    if (request.getURI() == "/register") {
        return new RegisterController(_container->resolve<AuthService>());
    }
    if (request.getURI() == "/login") {
        return new LoginController(_container->resolve<AuthService>());
    }
    return nullptr;
}
#include "AuthRouter.h"
#include "../Controllers/RegisterController.h"
#include "../Controllers/LoginController.h"
#include "../Controllers/WebSocketController.h"
#include "../Services/AuthService.h"
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/URI.h>

AuthRouter::AuthRouter(std::shared_ptr<DIContainer> container) : _container(container) {}

// POCO takes ownership of the returned raw pointer and deletes it when the request is done.
// We safely pass in our Singleton Services via the DI container.
Poco::Net::HTTPRequestHandler* AuthRouter::createRequestHandler(const Poco::Net::HTTPServerRequest& request) {
    Poco::URI uri(request.getURI());
    std::string path = uri.getPath();
    if (request.getMethod() == "POST") {
        if (path == "/api/register") {
            return new RegisterController(_container->resolve<AuthService>());
        }
        if (path == "/api/login") {
            return new LoginController(_container->resolve<AuthService>());
        }
    }
    else if (request.getMethod() == "GET") {
        if (path == "/api/ws") {
            return new WebSocketController(_container->resolve<AuthService>(), _container->resolve<ThreadSafeQueue<Command>>());
        }
    }
    return nullptr;
}
#include "LoginController.h"
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/JSON/Parser.h>

LoginController::LoginController(std::shared_ptr<AuthService> service) : _authService(service) {}

void LoginController::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) {
    try {
        Poco::JSON::Parser parser;
        auto jsonObject = parser.parse(request.stream()).extract<Poco::JSON::Object::Ptr>();
        
        std::string username = jsonObject->getValue<std::string>("username");
        std::string password = jsonObject->getValue<std::string>("password");
        
        std::string jwt = _authService->loginUser(username, password);
        
        response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
        response.setContentType("application/json");
        response.send() << "{\"token\":\"" << jwt << "\"}";
        
    } catch (const std::invalid_argument& exc) {
        response.setStatus(Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
        response.setContentType("application/json");
        response.send() << "{\"error\":\"" << exc.what() << "\"}";
    } catch (const Poco::Exception& exc) {
        response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        response.setContentType("application/json");
        response.send() << "{\"error\":\"Invalid Request\"}";
    }
}
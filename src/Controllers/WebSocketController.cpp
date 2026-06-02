#include "WebSocketController.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/NetException.h"
#include "Poco/URI.h"
#include "Poco/JSON/Parser.h"
#include "Poco/Dynamic/Var.h"
#include <iostream>

WebSocketController::WebSocketController(
    std::shared_ptr<AuthService> authService,
    std::shared_ptr<ThreadSafeQueue<Command>> commandQueue)
    : _authService(authService), _commandQueue(commandQueue) {
}

void WebSocketController::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) {
    try {
        std::string token = extractToken(request.getURI());
        std::string sub;

        if (token.empty() || !_authService->validateToken(token, sub)) {
            response.setStatus(Poco::Net::HTTPServerResponse::HTTP_UNAUTHORIZED);
            response.setContentType("text/plain");
            response.send() << "401 Unauthorized: Invalid or expired token.";
            return;
        }

        Poco::Net::WebSocket ws(request, response);
        std::cout << "[WebSocket] Connection authenticated and established with: " << sub << "!" << std::endl;

        char buffer[4096]; // Bumped up space for larger JSON structures
        int flags;
        int n;

        do {
            n = ws.receiveFrame(buffer, sizeof(buffer), flags);
            if (n > 0 && ((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) != Poco::Net::WebSocket::FRAME_OP_CLOSE)) {

                std::string receivedData(buffer, n);

                try {
                    Poco::JSON::Parser parser;
                    Poco::Dynamic::Var result = parser.parse(receivedData);
                    Poco::JSON::Object::Ptr jsonObject = result.extract<Poco::JSON::Object::Ptr>();

                    // Pack into our structure and hand off to the queue
                    Command newCmd;
                    newCmd.senderId = sub;
                    newCmd.payload = jsonObject;

                    _commandQueue->push(newCmd);

                }
                catch (const Poco::Exception& e) {
                    std::cerr << "[WebSocket JSON Error] From " << sub << ": " << e.displayText() << std::endl;
                }
                catch (const std::exception& e) {
                    std::cerr << "[WebSocket JSON Error] From " << sub << ": " << e.what() << std::endl;
                }
            }
        } while (n > 0 && ((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) != Poco::Net::WebSocket::FRAME_OP_CLOSE));

        std::cout << "[WebSocket] Connection closed gracefully with " << sub << "!" << std::endl;
    }
    catch (Poco::Exception& exc) {
        std::cerr << "[Server Error] " << exc.displayText() << std::endl;
        if (!response.sent()) {
            response.setStatus(Poco::Net::HTTPServerResponse::HTTP_INTERNAL_SERVER_ERROR);
            response.send();
        }
    }
}

std::string WebSocketController::extractToken(const std::string& uriString) {
    Poco::URI uri(uriString);
    auto queryParameters = uri.getQueryParameters();
    for (const auto& param : queryParameters) {
        if (param.first == "token") {
            return param.second;
        }
    }
    return "";
}
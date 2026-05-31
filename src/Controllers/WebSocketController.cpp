#include "WebSocketController.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/NetException.h"
#include "Poco/URI.h"
#include <iostream>

WebSocketController::WebSocketController(std::shared_ptr<AuthService> authService)
    : _authService(authService) {
}

void WebSocketController::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) {
    try {
        std::string token = extractToken(request.getURI());

        if (token.empty() || !_authService->validateToken(token)) {
            response.setStatus(Poco::Net::HTTPServerResponse::HTTP_UNAUTHORIZED);
            response.setContentType("text/plain");
            response.send() << "401 Unauthorized: Invalid or expired token.";
            return;
        }

        Poco::Net::WebSocket ws(request, response);
        std::cout << "[WebSocket] Connection authenticated and established!" << std::endl;

        char buffer[1024];
        int flags;
        int n;

        do {
            n = ws.receiveFrame(buffer, sizeof(buffer), flags);
            if (n > 0 && ((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) != Poco::Net::WebSocket::FRAME_OP_CLOSE)) {
                ws.sendFrame(buffer, n, flags);
            }
        } while (n > 0 && ((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) != Poco::Net::WebSocket::FRAME_OP_CLOSE));

        std::cout << "[WebSocket] Connection closed gracefully." << std::endl;
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
#include "AuthService.h"
#include "bcrypt.h"
#include <Poco/JWT/Token.h>
#include <Poco/JWT/Signer.h>
#include <stdexcept>
#include <iostream>

AuthService::AuthService(std::shared_ptr<UserRepository> repo, std::shared_ptr<AppConfig> config) 
    : _userRepo(repo), _config(config) {}

void AuthService::registerUser(const std::string& username, const std::string& password) {
    if (_userRepo->findByUsername(username).has_value()) {
        throw std::invalid_argument("Username already exists");
    }

    User newUser;
    newUser.username = username;
    newUser.passwordHash = bcrypt::generateHash(password, 12);
    _userRepo->save(newUser);

    // Contextual "Other Work"
    std::cout << "[Service] Provisioning default workspace for: " << username << "\n";
}

std::string AuthService::loginUser(const std::string& username, const std::string& password) {
    auto userOpt = _userRepo->findByUsername(username);
    
    if (!userOpt.has_value() || !bcrypt::validatePassword(password, userOpt->passwordHash)) {
        throw std::invalid_argument("Invalid credentials");
    }

    Poco::JWT::Token token;
    token.setSubject(username);
    token.setExpiration(Poco::Timestamp() + Poco::Timespan(0, _config->jwtExpHours, 0, 0, 0));
    
    Poco::JWT::Signer signer(_config->jwtSecret);
    return signer.sign(token, Poco::JWT::Signer::ALGO_HS256);
}

bool AuthService::validateToken(const std::string& jwt, std::string sub) {
    Poco::JWT::Signer signer(_config->jwtSecret);
    try {
        Poco::JWT::Token token = signer.verify(jwt);
        sub = token.getSubject();
        return true;
    }
    catch (Poco::Exception& ex) {
        return false;
    }
}
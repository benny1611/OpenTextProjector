#pragma once
#include "../Repositories/UserRepository.h"
#include "../Core/AppConfig.h"
#include <memory>
#include <string>

class AuthService {
private:
    std::shared_ptr<UserRepository> _userRepo;
    std::shared_ptr<AppConfig> _config;

public:
    AuthService(std::shared_ptr<UserRepository> repo, std::shared_ptr<AppConfig> config);
    void registerUser(const std::string& username, const std::string& password);
    std::string loginUser(const std::string& username, const std::string& password);
    bool validateToken(const std::string& token, std::string& sub);
};
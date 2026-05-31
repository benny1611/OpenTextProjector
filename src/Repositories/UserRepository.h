#pragma once
#include "../Models/User.h"
#include <Poco/Data/SessionPool.h>
#include <memory>
#include <optional>

class UserRepository {
private:
    std::shared_ptr<Poco::Data::SessionPool> _pool;

public:
    explicit UserRepository(std::shared_ptr<Poco::Data::SessionPool> pool);
    void save(const User& user);
    std::optional<User> findByUsername(const std::string& username);
};
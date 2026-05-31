#include "UserRepository.h"
#include <Poco/Data/Session.h>

using namespace Poco::Data::Keywords;

UserRepository::UserRepository(std::shared_ptr<Poco::Data::SessionPool> pool) : _pool(pool) {}

void UserRepository::save(const User& user) {
    Poco::Data::Session session(_pool->get());
    session << "INSERT INTO users (username, password_hash) VALUES (?, ?)", 
        useRef(user.username), useRef(user.passwordHash), now;
}

std::optional<User> UserRepository::findByUsername(const std::string& username) {
    Poco::Data::Session session(_pool->get());
    User user;
    
    Poco::Data::Statement select(session);
    select << "SELECT id, username, password_hash FROM users WHERE username = ?",
        into(user.id), into(user.username), into(user.passwordHash), useRef(username);
        
    if (select.execute() > 0) {
        return user;
    }
    return std::nullopt;
}
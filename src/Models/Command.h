#pragma once
#include <string>
#include "Poco/JSON/Object.h"

struct Command {
    std::string senderId;
    Poco::JSON::Object::Ptr payload;
};
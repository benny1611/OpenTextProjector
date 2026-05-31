#pragma once
#include <string>

// A simple DTO to hold our environment properties so we don't 
// have to inject the entire POCO Configuration object everywhere.
struct AppConfig {
    std::string jwtSecret;
    int jwtExpHours;
};
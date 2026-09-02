#include <drogon/drogon.h>

#include "JwtService.h"

#include <fstream>
#include <iostream>
#include <json/json.h>

namespace
{
void loadAppConfig()
{
    std::ifstream in("config/app.json");
    if (!in)
    {
        std::cerr << "Warning: config/app.json not found; using default JWT secret\n";
        JwtService::configure("dev-only-change-me", 3600);
        return;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, in, &root, &errs))
    {
        throw std::runtime_error("Failed to parse config/app.json: " + errs);
    }

    const std::string secret =
        root.get("jwt_secret", "dev-only-change-me").asString();
    const int expires = root.get("jwt_expires_seconds", 3600).asInt();
    JwtService::configure(secret, expires);
}
}  // namespace

int main()
{
    try
    {
        loadAppConfig();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    drogon::app()
        .loadConfigFile("config/config.json")
        .registerBeginningAdvice([]() {
            try
            {
                auto db = drogon::app().getDbClient("default");
                *db << "SELECT 1 AS ok"
                    >> [](const drogon::orm::Result& result) {
                           std::cerr << "Database connected. SELECT 1 = "
                                     << result[0]["ok"].as<int>() << '\n';
                       }
                    >> [](const drogon::orm::DrogonDbException& e) {
                           std::cerr << "Database error: " << e.base().what()
                                     << '\n';
                       };
            }
            catch (const std::exception& e)
            {
                std::cerr << "Failed to get DB client: " << e.what() << '\n';
            }
        })
        .addListener("0.0.0.0", 8080)
        .run();

    return 0;
}

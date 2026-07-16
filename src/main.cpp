#include <drogon/drogon.h>

#include <iostream>

int main()
{
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

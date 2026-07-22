#pragma once

#include <drogon/HttpController.h>

#include <string>

class UrlController : public drogon::HttpController<UrlController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UrlController::shorten, "/shorten", drogon::Post);
    METHOD_LIST_END

    void shorten(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

  private:
    static drogon::HttpResponsePtr badRequest(const std::string& message);
    static drogon::HttpResponsePtr serverError(const std::string& message);

    /// Generates a unique short code, inserts the row, then responds with short_code.
    static void createShortUrl(
        std::string originalUrl,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        int attempt = 0);
};

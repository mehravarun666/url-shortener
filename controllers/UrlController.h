#pragma once

#include <drogon/HttpController.h>
#include <trantor/utils/Date.h>

#include <optional>
#include <string>

class UrlController : public drogon::HttpController<UrlController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UrlController::shorten, "/shorten", drogon::Post);
    ADD_METHOD_TO(UrlController::redirect, "/{short_code}", drogon::Get);
    METHOD_LIST_END

    void shorten(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

    void redirect(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                  const std::string& shortCode) const;

  private:
    static drogon::HttpResponsePtr badRequest(const std::string& message);
    static drogon::HttpResponsePtr notFound(const std::string& message);
    static drogon::HttpResponsePtr conflict(const std::string& message);
    static drogon::HttpResponsePtr gone(const std::string& message);
    static drogon::HttpResponsePtr serverError(const std::string& message);

    /// Inserts with a caller-provided short code (custom alias path).
    static void insertWithCode(
        std::string originalUrl,
        std::string shortCode,
        std::optional<trantor::Date> expiresAt,
        std::function<void(const drogon::HttpResponsePtr&)> callback);

    /// Generates a unique short code, inserts the row, then responds with short_code.
    static void createShortUrl(
        std::string originalUrl,
        std::optional<trantor::Date> expiresAt,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        int attempt = 0);
};

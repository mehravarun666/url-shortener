#pragma once

#include <drogon/HttpController.h>
#include <trantor/utils/Date.h>

#include <optional>
#include <string>

class UrlController : public drogon::HttpController<UrlController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UrlController::shorten, "/shorten", drogon::Post, "JwtFilter");
    ADD_METHOD_TO(UrlController::listUrls, "/api/v1/urls", drogon::Get, "JwtFilter");
    ADD_METHOD_TO(UrlController::getUrl, "/api/v1/urls/{id}", drogon::Get, "JwtFilter");
    ADD_METHOD_TO(UrlController::updateUrl, "/api/v1/urls/{id}", drogon::Patch, "JwtFilter");
    ADD_METHOD_TO(UrlController::deleteUrl, "/api/v1/urls/{id}", drogon::Delete, "JwtFilter");
    ADD_METHOD_TO(UrlController::redirect, "/{short_code}", drogon::Get);
    METHOD_LIST_END

    void shorten(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

    void listUrls(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

    void getUrl(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                const std::string& id) const;

    void updateUrl(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   const std::string& id) const;

    void deleteUrl(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   const std::string& id) const;

    void redirect(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                  const std::string& shortCode) const;

  private:
    static drogon::HttpResponsePtr badRequest(const std::string& message);
    static drogon::HttpResponsePtr notFound(const std::string& message);
    static drogon::HttpResponsePtr conflict(const std::string& message);
    static drogon::HttpResponsePtr gone(const std::string& message);
    static drogon::HttpResponsePtr serverError(const std::string& message);

    static void insertWithCode(
        std::string originalUrl,
        std::string shortCode,
        std::optional<trantor::Date> expiresAt,
        int32_t userId,
        std::function<void(const drogon::HttpResponsePtr&)> callback);

    static void createShortUrl(
        std::string originalUrl,
        std::optional<trantor::Date> expiresAt,
        int32_t userId,
        std::function<void(const drogon::HttpResponsePtr&)> callback,
        int attempt = 0);
};

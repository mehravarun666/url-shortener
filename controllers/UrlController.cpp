#include "UrlController.h"
#include "ShortCodeGenerator.h"
#include "Urls.h"

#include <drogon/orm/Criteria.h>
#include <drogon/orm/Mapper.h>

#include <json/json.h>

using drogon_model::url_shortener::Urls;

namespace
{
constexpr int kMaxCollisionRetries = 8;

bool startsWithHttpScheme(const std::string& url)
{
    return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
}
}  // namespace

drogon::HttpResponsePtr UrlController::badRequest(const std::string& message)
{
    Json::Value body;
    body["error"]["code"] = "VALIDATION_ERROR";
    body["error"]["message"] = message;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(drogon::k400BadRequest);
    return resp;
}

drogon::HttpResponsePtr UrlController::serverError(const std::string& message)
{
    Json::Value body;
    body["error"]["code"] = "INTERNAL_ERROR";
    body["error"]["message"] = message;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(drogon::k500InternalServerError);
    return resp;
}

void UrlController::createShortUrl(
    std::string originalUrl,
    std::function<void(const drogon::HttpResponsePtr&)> callback,
    int attempt)
{
    if (attempt >= kMaxCollisionRetries)
    {
        callback(serverError("Could not generate a unique short code"));
        return;
    }

    const std::string code = ShortCodeGenerator::generate();
    auto db = drogon::app().getDbClient("default");
    drogon::orm::Mapper<Urls> mapper(db);

    mapper.findBy(
        drogon::orm::Criteria(Urls::Cols::_short_code,
                              drogon::orm::CompareOperator::EQ,
                              code),
        [originalUrl = std::move(originalUrl),
         callback = std::move(callback),
         code,
         attempt](const std::vector<Urls>& existing) mutable {
            if (!existing.empty())
            {
                // Collision — try another code.
                createShortUrl(std::move(originalUrl),
                               std::move(callback),
                               attempt + 1);
                return;
            }

            Urls row;
            row.setOriginalUrl(originalUrl);
            row.setShortCode(code);

            drogon::orm::Mapper<Urls> insertMapper(
                drogon::app().getDbClient("default"));
            insertMapper.insert(
                row,
                [callback](const Urls& saved) {
                    Json::Value body;
                    body["short_code"] = saved.getValueOfShortCode();
                    callback(drogon::HttpResponse::newHttpJsonResponse(body));
                },
                [originalUrl = std::move(originalUrl),
                 callback,
                 attempt](const drogon::orm::DrogonDbException& e) mutable {
                    const std::string msg = e.base().what();
                    // Rare race: another request inserted the same code.
                    if (msg.find("unique") != std::string::npos ||
                        msg.find("duplicate") != std::string::npos)
                    {
                        createShortUrl(std::move(originalUrl),
                                       std::move(callback),
                                       attempt + 1);
                        return;
                    }
                    callback(serverError(msg));
                });
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            callback(serverError(e.base().what()));
        });
}

void UrlController::shorten(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const
{
    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr)
    {
        callback(badRequest("Malformed JSON body"));
        return;
    }

    if (!jsonPtr->isMember("url") || !(*jsonPtr)["url"].isString())
    {
        callback(badRequest("Missing required field: url"));
        return;
    }

    const std::string url = (*jsonPtr)["url"].asString();
    if (url.empty())
    {
        callback(badRequest("url must not be empty"));
        return;
    }

    if (!startsWithHttpScheme(url))
    {
        callback(badRequest("url must start with http:// or https://"));
        return;
    }

    createShortUrl(url, std::move(callback));
}

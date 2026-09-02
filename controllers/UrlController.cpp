#include "UrlController.h"
#include "ShortCodeGenerator.h"
#include "Urls.h"

#include <drogon/orm/Criteria.h>
#include <drogon/orm/Mapper.h>

#include <json/json.h>

#include <algorithm>
#include <cctype>

using drogon_model::url_shortener::Urls;

namespace
{
constexpr int kMaxCollisionRetries = 8;
constexpr std::size_t kMaxAliasLength = 10;

bool startsWithHttpScheme(const std::string& url)
{
    return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
}

bool isValidAlias(const std::string& alias)
{
    if (alias.empty() || alias.size() > kMaxAliasLength)
    {
        return false;
    }
    return std::all_of(alias.begin(), alias.end(), [](unsigned char c) {
        return std::isalnum(c) != 0;
    });
}

bool isReservedAlias(const std::string& alias)
{
    std::string lower;
    lower.reserve(alias.size());
    for (unsigned char c : alias)
    {
        lower.push_back(static_cast<char>(std::tolower(c)));
    }
    return lower == "health" || lower == "shorten";
}

void applyExpiresAt(Urls& row, const std::optional<trantor::Date>& expiresAt)
{
    if (expiresAt)
    {
        row.setExpiresAt(*expiresAt);
    }
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

drogon::HttpResponsePtr UrlController::notFound(const std::string& message)
{
    Json::Value body;
    body["error"]["code"] = "NOT_FOUND";
    body["error"]["message"] = message;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(drogon::k404NotFound);
    return resp;
}

drogon::HttpResponsePtr UrlController::conflict(const std::string& message)
{
    Json::Value body;
    body["error"]["code"] = "CONFLICT";
    body["error"]["message"] = message;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(drogon::k409Conflict);
    return resp;
}

drogon::HttpResponsePtr UrlController::gone(const std::string& message)
{
    Json::Value body;
    body["error"]["code"] = "GONE";
    body["error"]["message"] = message;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(drogon::k410Gone);
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

void UrlController::insertWithCode(
    std::string originalUrl,
    std::string shortCode,
    std::optional<trantor::Date> expiresAt,
    std::function<void(const drogon::HttpResponsePtr&)> callback)
{
    Urls row;
    row.setOriginalUrl(originalUrl);
    row.setShortCode(shortCode);
    applyExpiresAt(row, expiresAt);

    drogon::orm::Mapper<Urls> mapper(drogon::app().getDbClient("default"));
    mapper.insert(
        row,
        [callback](const Urls& saved) {
            Json::Value body;
            body["short_code"] = saved.getValueOfShortCode();
            callback(drogon::HttpResponse::newHttpJsonResponse(body));
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            const std::string msg = e.base().what();
            if (msg.find("unique") != std::string::npos ||
                msg.find("duplicate") != std::string::npos)
            {
                callback(conflict("custom_alias is already taken"));
                return;
            }
            callback(serverError(msg));
        });
}

void UrlController::createShortUrl(
    std::string originalUrl,
    std::optional<trantor::Date> expiresAt,
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
         expiresAt = std::move(expiresAt),
         callback = std::move(callback),
         code,
         attempt](const std::vector<Urls>& existing) mutable {
            if (!existing.empty())
            {
                createShortUrl(std::move(originalUrl),
                               std::move(expiresAt),
                               std::move(callback),
                               attempt + 1);
                return;
            }

            Urls row;
            row.setOriginalUrl(originalUrl);
            row.setShortCode(code);
            applyExpiresAt(row, expiresAt);

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
                 expiresAt = std::move(expiresAt),
                 callback,
                 attempt](const drogon::orm::DrogonDbException& e) mutable {
                    const std::string msg = e.base().what();
                    if (msg.find("unique") != std::string::npos ||
                        msg.find("duplicate") != std::string::npos)
                    {
                        createShortUrl(std::move(originalUrl),
                                       std::move(expiresAt),
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

    std::optional<std::string> customAlias;
    if (jsonPtr->isMember("custom_alias"))
    {
        if (!(*jsonPtr)["custom_alias"].isString())
        {
            callback(badRequest("custom_alias must be a string"));
            return;
        }
        const std::string alias = (*jsonPtr)["custom_alias"].asString();
        if (!isValidAlias(alias))
        {
            callback(badRequest(
                "alias must be 1-10 alphanumeric characters"));
            return;
        }
        if (isReservedAlias(alias))
        {
            callback(badRequest("custom_alias is reserved"));
            return;
        }
        customAlias = alias;
    }

    std::optional<trantor::Date> expiresAt;
    if (jsonPtr->isMember("expires_at"))
    {
        if (!(*jsonPtr)["expires_at"].isString())
        {
            callback(badRequest("expires_at must be an ISO-8601 string"));
            return;
        }
        const std::string raw = (*jsonPtr)["expires_at"].asString();
        trantor::Date parsed;
        try
        {
            parsed = trantor::Date::fromISOString(raw);
        }
        catch (const std::exception&)
        {
            callback(badRequest("invalid or past expires_at"));
            return;
        }
        if (parsed.microSecondsSinceEpoch() == 0 ||
            parsed <= trantor::Date::now())
        {
            callback(badRequest("invalid or past expires_at"));
            return;
        }
        expiresAt = parsed;
    }

    if (customAlias)
    {
        auto db = drogon::app().getDbClient("default");
        drogon::orm::Mapper<Urls> mapper(db);
        const std::string alias = *customAlias;

        mapper.findBy(
            drogon::orm::Criteria(Urls::Cols::_short_code,
                                  drogon::orm::CompareOperator::EQ,
                                  alias),
            [url, alias, expiresAt = std::move(expiresAt),
             callback = std::move(callback)](
                const std::vector<Urls>& existing) mutable {
                if (!existing.empty())
                {
                    callback(conflict("custom_alias is already taken"));
                    return;
                }
                insertWithCode(std::move(url),
                               std::move(alias),
                               std::move(expiresAt),
                               std::move(callback));
            },
            [callback = std::move(callback)](
                const drogon::orm::DrogonDbException& e) {
                callback(serverError(e.base().what()));
            });
        return;
    }

    createShortUrl(url, std::move(expiresAt), std::move(callback));
}

void UrlController::redirect(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& shortCode) const
{
    if (shortCode.empty())
    {
        callback(notFound("Short code not found"));
        return;
    }

    auto db = drogon::app().getDbClient("default");
    drogon::orm::Mapper<Urls> mapper(db);

    mapper.findBy(
        drogon::orm::Criteria(Urls::Cols::_short_code,
                              drogon::orm::CompareOperator::EQ,
                              shortCode),
        [callback](const std::vector<Urls>& rows) {
            if (rows.empty())
            {
                callback(notFound("Short code not found"));
                return;
            }

            const Urls& row = rows.front();
            if (row.getExpiresAt())
            {
                if (*row.getExpiresAt() <= trantor::Date::now())
                {
                    callback(gone("short URL has expired"));
                    return;
                }
            }

            const std::string destination = row.getValueOfOriginalUrl();
            const int32_t urlId = row.getValueOfId();

            auto dbClient = drogon::app().getDbClient("default");
            *dbClient << "UPDATE urls SET click_count = click_count + 1 WHERE id = $1"
                      << urlId
                      >> [](const drogon::orm::Result&) {}
                      >> [](const drogon::orm::DrogonDbException& e) {
                             LOG_ERROR << "Failed to increment click_count: "
                                       << e.base().what();
                         };

            callback(drogon::HttpResponse::newRedirectionResponse(destination));
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            callback(serverError(e.base().what()));
        });
}

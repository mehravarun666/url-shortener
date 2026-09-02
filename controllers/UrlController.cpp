#include "UrlController.h"
#include "ShortCodeGenerator.h"
#include "Urls.h"

#include <drogon/orm/Criteria.h>
#include <drogon/orm/Mapper.h>

#include <json/json.h>

#include <algorithm>
#include <cctype>
#include <limits>

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

Json::Value urlToJson(const Urls& row)
{
    Json::Value body;
    body["id"] = row.getValueOfId();
    if (row.getUserId())
    {
        body["user_id"] = *row.getUserId();
    }
    else
    {
        body["user_id"] = Json::nullValue;
    }
    body["original_url"] = row.getValueOfOriginalUrl();
    body["short_code"] = row.getValueOfShortCode();
    body["click_count"] = row.getValueOfClickCount();
    if (row.getCreatedAt())
    {
        body["created_at"] = row.getCreatedAt()->toDbString();
    }
    else
    {
        body["created_at"] = Json::nullValue;
    }
    if (row.getExpiresAt())
    {
        body["expires_at"] = row.getExpiresAt()->toDbString();
    }
    else
    {
        body["expires_at"] = Json::nullValue;
    }
    return body;
}

bool ownedBy(const Urls& row, int32_t userId)
{
    return row.getUserId() && *row.getUserId() == userId;
}

bool parseId(const std::string& raw, int32_t& out)
{
    if (raw.empty())
    {
        return false;
    }
    try
    {
        size_t idx = 0;
        const long value = std::stol(raw, &idx);
        if (idx != raw.size() || value <= 0 ||
            value > static_cast<long>(std::numeric_limits<int32_t>::max()))
        {
            return false;
        }
        out = static_cast<int32_t>(value);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
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
    int32_t userId,
    std::function<void(const drogon::HttpResponsePtr&)> callback)
{
    Urls row;
    row.setOriginalUrl(originalUrl);
    row.setShortCode(shortCode);
    row.setUserId(userId);
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
    int32_t userId,
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
         userId,
         callback = std::move(callback),
         code,
         attempt](const std::vector<Urls>& existing) mutable {
            if (!existing.empty())
            {
                createShortUrl(std::move(originalUrl),
                               std::move(expiresAt),
                               userId,
                               std::move(callback),
                               attempt + 1);
                return;
            }

            Urls row;
            row.setOriginalUrl(originalUrl);
            row.setShortCode(code);
            row.setUserId(userId);
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
                 userId,
                 callback,
                 attempt](const drogon::orm::DrogonDbException& e) mutable {
                    const std::string msg = e.base().what();
                    if (msg.find("unique") != std::string::npos ||
                        msg.find("duplicate") != std::string::npos)
                    {
                        createShortUrl(std::move(originalUrl),
                                       std::move(expiresAt),
                                       userId,
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
        const int32_t userId = req->attributes()->get<int32_t>("user_id");

        mapper.findBy(
            drogon::orm::Criteria(Urls::Cols::_short_code,
                                  drogon::orm::CompareOperator::EQ,
                                  alias),
            [url, alias, expiresAt = std::move(expiresAt), userId,
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
                               userId,
                               std::move(callback));
            },
            [callback = std::move(callback)](
                const drogon::orm::DrogonDbException& e) {
                callback(serverError(e.base().what()));
            });
        return;
    }

    createShortUrl(url,
                   std::move(expiresAt),
                   req->attributes()->get<int32_t>("user_id"),
                   std::move(callback));
}

void UrlController::listUrls(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const
{
    size_t page = 1;
    size_t limit = 20;
    try
    {
        if (!req->getParameter("page").empty())
        {
            page = static_cast<size_t>(std::stoul(req->getParameter("page")));
        }
        if (!req->getParameter("limit").empty())
        {
            limit = static_cast<size_t>(std::stoul(req->getParameter("limit")));
        }
    }
    catch (const std::exception&)
    {
        callback(badRequest("page and limit must be positive integers"));
        return;
    }
    if (page == 0 || limit == 0 || limit > 100)
    {
        callback(badRequest("page must be >= 1 and limit must be 1-100"));
        return;
    }

    const std::string q = req->getParameter("q");
    const int32_t userId = req->attributes()->get<int32_t>("user_id");
    drogon::orm::Criteria criteria(
        Urls::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId);
    if (!q.empty())
    {
        criteria = criteria && drogon::orm::Criteria(
                                   Urls::Cols::_original_url,
                                   drogon::orm::CompareOperator::Like,
                                   "%" + q + "%");
    }

    drogon::orm::Mapper<Urls> countMapper(drogon::app().getDbClient("default"));
    countMapper.count(
        criteria,
        [page, limit, criteria, callback = std::move(callback)](size_t total) {
            drogon::orm::Mapper<Urls> listMapper(
                drogon::app().getDbClient("default"));
            listMapper.orderBy(Urls::Cols::_id, drogon::orm::SortOrder::DESC)
                .paginate(page, limit)
                .findBy(
                    criteria,
                    [total, page, limit, callback](const std::vector<Urls>& rows) {
                        Json::Value body;
                        body["data"] = Json::arrayValue;
                        for (const auto& row : rows)
                        {
                            body["data"].append(urlToJson(row));
                        }
                        body["pagination"]["page"] = static_cast<Json::UInt64>(page);
                        body["pagination"]["limit"] =
                            static_cast<Json::UInt64>(limit);
                        body["pagination"]["total"] =
                            static_cast<Json::UInt64>(total);
                        callback(drogon::HttpResponse::newHttpJsonResponse(body));
                    },
                    [callback](const drogon::orm::DrogonDbException& e) {
                        callback(serverError(e.base().what()));
                    });
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            callback(serverError(e.base().what()));
        });
}

void UrlController::getUrl(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) const
{
    int32_t urlId = 0;
    if (!parseId(id, urlId))
    {
        callback(badRequest("id must be a positive integer"));
        return;
    }

    drogon::orm::Mapper<Urls> mapper(drogon::app().getDbClient("default"));
    const int32_t currentUserId = req->attributes()->get<int32_t>("user_id");
    mapper.findByPrimaryKey(
        urlId,
        [callback, currentUserId](Urls row) {
            if (!ownedBy(row, currentUserId))
            {
                callback(notFound("URL not found"));
                return;
            }
            callback(drogon::HttpResponse::newHttpJsonResponse(urlToJson(row)));
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            const std::string msg = e.base().what();
            if (msg.find("0 rows") != std::string::npos ||
                msg.find("Unexpected rows") != std::string::npos ||
                msg.find("not found") != std::string::npos)
            {
                callback(notFound("URL not found"));
                return;
            }
            callback(serverError(msg));
        });
}

void UrlController::updateUrl(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) const
{
    int32_t urlId = 0;
    if (!parseId(id, urlId))
    {
        callback(badRequest("id must be a positive integer"));
        return;
    }

    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr)
    {
        callback(badRequest("Malformed JSON body"));
        return;
    }

    const int32_t currentUserId = req->attributes()->get<int32_t>("user_id");
    drogon::orm::Mapper<Urls> mapper(drogon::app().getDbClient("default"));
    mapper.findByPrimaryKey(
        urlId,
        [jsonPtr, callback = std::move(callback),
         currentUserId](Urls row) mutable {
            if (!ownedBy(row, currentUserId))
            {
                callback(notFound("URL not found"));
                return;
            }
            if (jsonPtr->isMember("original_url"))
            {
                if (!(*jsonPtr)["original_url"].isString())
                {
                    callback(badRequest("original_url must be a string"));
                    return;
                }
                const std::string original = (*jsonPtr)["original_url"].asString();
                if (original.empty() || !startsWithHttpScheme(original))
                {
                    callback(badRequest(
                        "original_url must be a non-empty http(s) URL"));
                    return;
                }
                row.setOriginalUrl(original);
            }

            if (jsonPtr->isMember("custom_alias"))
            {
                if (!(*jsonPtr)["custom_alias"].isString())
                {
                    callback(badRequest("custom_alias must be a string"));
                    return;
                }
                const std::string alias = (*jsonPtr)["custom_alias"].asString();
                if (!isValidAlias(alias) || isReservedAlias(alias))
                {
                    callback(badRequest(
                        "custom_alias must be 1-10 alphanumeric and not reserved"));
                    return;
                }
                row.setShortCode(alias);
            }

            if (jsonPtr->isMember("expires_at"))
            {
                if ((*jsonPtr)["expires_at"].isNull())
                {
                    row.setExpiresAtToNull();
                }
                else if ((*jsonPtr)["expires_at"].isString())
                {
                    try
                    {
                        const auto parsed = trantor::Date::fromISOString(
                            (*jsonPtr)["expires_at"].asString());
                        if (parsed.microSecondsSinceEpoch() == 0 ||
                            parsed <= trantor::Date::now())
                        {
                            callback(badRequest("invalid or past expires_at"));
                            return;
                        }
                        row.setExpiresAt(parsed);
                    }
                    catch (const std::exception&)
                    {
                        callback(badRequest("invalid or past expires_at"));
                        return;
                    }
                }
                else
                {
                    callback(badRequest("expires_at must be a string or null"));
                    return;
                }
            }

            drogon::orm::Mapper<Urls> updateMapper(
                drogon::app().getDbClient("default"));
            updateMapper.update(
                row,
                [row, callback](size_t) {
                    callback(
                        drogon::HttpResponse::newHttpJsonResponse(urlToJson(row)));
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
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            const std::string msg = e.base().what();
            if (msg.find("0 rows") != std::string::npos ||
                msg.find("Unexpected rows") != std::string::npos ||
                msg.find("not found") != std::string::npos)
            {
                callback(notFound("URL not found"));
                return;
            }
            callback(serverError(msg));
        });
}

void UrlController::deleteUrl(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& id) const
{
    int32_t urlId = 0;
    if (!parseId(id, urlId))
    {
        callback(badRequest("id must be a positive integer"));
        return;
    }

    const int32_t currentUserId = req->attributes()->get<int32_t>("user_id");
    drogon::orm::Mapper<Urls> mapper(drogon::app().getDbClient("default"));
    mapper.findByPrimaryKey(
        urlId,
        [callback = std::move(callback), currentUserId](Urls row) mutable {
            if (!ownedBy(row, currentUserId))
            {
                callback(notFound("URL not found"));
                return;
            }
            drogon::orm::Mapper<Urls> deleteMapper(
                drogon::app().getDbClient("default"));
            deleteMapper.deleteByPrimaryKey(
                row.getValueOfId(),
                [callback](size_t count) {
                    if (count == 0)
                    {
                        callback(notFound("URL not found"));
                        return;
                    }
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k204NoContent);
                    callback(resp);
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    callback(serverError(e.base().what()));
                });
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            const std::string msg = e.base().what();
            if (msg.find("0 rows") != std::string::npos ||
                msg.find("Unexpected rows") != std::string::npos ||
                msg.find("not found") != std::string::npos)
            {
                callback(notFound("URL not found"));
                return;
            }
            callback(serverError(msg));
        });
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

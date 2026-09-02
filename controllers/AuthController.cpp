#include "AuthController.h"
#include "JwtService.h"
#include "PasswordHasher.h"
#include "Users.h"

#include <drogon/orm/Criteria.h>
#include <drogon/orm/Mapper.h>

#include <json/json.h>

using drogon_model::url_shortener::Users;

namespace
{
drogon::HttpResponsePtr jsonError(drogon::HttpStatusCode code,
                                  const std::string& errorCode,
                                  const std::string& message)
{
    Json::Value body;
    body["error"]["code"] = errorCode;
    body["error"]["message"] = message;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(code);
    return resp;
}

Json::Value userToJson(const Users& user)
{
    Json::Value body;
    body["id"] = user.getValueOfId();
    body["email"] = user.getValueOfEmail();
    body["name"] = user.getValueOfName();
    if (user.getCreatedAt())
    {
        body["created_at"] = user.getCreatedAt()->toDbString();
    }
    else
    {
        body["created_at"] = Json::nullValue;
    }
    return body;
}
}  // namespace

void AuthController::registerUser(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const
{
    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr)
    {
        callback(jsonError(drogon::k400BadRequest, "VALIDATION_ERROR",
                           "Malformed JSON body"));
        return;
    }

    if (!jsonPtr->isMember("email") || !(*jsonPtr)["email"].isString() ||
        !jsonPtr->isMember("password") || !(*jsonPtr)["password"].isString() ||
        !jsonPtr->isMember("name") || !(*jsonPtr)["name"].isString())
    {
        callback(jsonError(drogon::k400BadRequest, "VALIDATION_ERROR",
                           "email, password, and name are required"));
        return;
    }

    const std::string email = (*jsonPtr)["email"].asString();
    const std::string password = (*jsonPtr)["password"].asString();
    const std::string name = (*jsonPtr)["name"].asString();

    if (email.empty() || name.empty())
    {
        callback(jsonError(drogon::k400BadRequest, "VALIDATION_ERROR",
                           "email and name must not be empty"));
        return;
    }
    if (password.size() < 8)
    {
        callback(jsonError(drogon::k400BadRequest, "VALIDATION_ERROR",
                           "password must be at least 8 characters"));
        return;
    }

    std::string passwordHash;
    try
    {
        passwordHash = PasswordHasher::hash(password);
    }
    catch (const std::exception& e)
    {
        callback(jsonError(drogon::k500InternalServerError, "INTERNAL_ERROR",
                           e.what()));
        return;
    }

    Users user;
    user.setEmail(email);
    user.setPasswordHash(passwordHash);
    user.setName(name);

    drogon::orm::Mapper<Users> mapper(drogon::app().getDbClient("default"));
    mapper.insert(
        user,
        [callback](Users saved) {
            auto resp =
                drogon::HttpResponse::newHttpJsonResponse(userToJson(saved));
            resp->setStatusCode(drogon::k201Created);
            callback(resp);
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            const std::string msg = e.base().what();
            if (msg.find("unique") != std::string::npos ||
                msg.find("duplicate") != std::string::npos)
            {
                callback(jsonError(drogon::k409Conflict, "CONFLICT",
                                   "email is already registered"));
                return;
            }
            callback(jsonError(drogon::k500InternalServerError, "INTERNAL_ERROR",
                               msg));
        });
}

void AuthController::login(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const
{
    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr)
    {
        callback(jsonError(drogon::k400BadRequest, "VALIDATION_ERROR",
                           "Malformed JSON body"));
        return;
    }
    if (!jsonPtr->isMember("email") || !(*jsonPtr)["email"].isString() ||
        !jsonPtr->isMember("password") || !(*jsonPtr)["password"].isString())
    {
        callback(jsonError(drogon::k400BadRequest, "VALIDATION_ERROR",
                           "email and password are required"));
        return;
    }

    const std::string email = (*jsonPtr)["email"].asString();
    const std::string password = (*jsonPtr)["password"].asString();

    drogon::orm::Mapper<Users> mapper(drogon::app().getDbClient("default"));
    mapper.findBy(
        drogon::orm::Criteria(Users::Cols::_email,
                              drogon::orm::CompareOperator::EQ,
                              email),
        [password, callback = std::move(callback)](const std::vector<Users>& rows) {
            if (rows.empty() ||
                !PasswordHasher::verify(password, rows.front().getValueOfPasswordHash()))
            {
                callback(jsonError(drogon::k401Unauthorized, "UNAUTHORIZED",
                                   "Invalid email or password"));
                return;
            }

            const auto& user = rows.front();
            Json::Value body;
            body["access_token"] =
                JwtService::createToken(user.getValueOfId(), user.getValueOfEmail());
            body["token_type"] = "Bearer";
            body["expires_in"] = JwtService::expiresSeconds();
            callback(drogon::HttpResponse::newHttpJsonResponse(body));
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            callback(jsonError(drogon::k500InternalServerError, "INTERNAL_ERROR",
                               e.base().what()));
        });
}

void AuthController::me(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const
{
    const auto userIdAttr = req->attributes()->get<int32_t>("user_id");
    drogon::orm::Mapper<Users> mapper(drogon::app().getDbClient("default"));
    mapper.findByPrimaryKey(
        userIdAttr,
        [callback](Users user) {
            callback(drogon::HttpResponse::newHttpJsonResponse(userToJson(user)));
        },
        [callback](const drogon::orm::DrogonDbException&) {
            callback(jsonError(drogon::k401Unauthorized, "UNAUTHORIZED",
                               "User not found"));
        });
}

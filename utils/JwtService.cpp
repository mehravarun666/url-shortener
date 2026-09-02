#include "JwtService.h"

#include <jwt-cpp/traits/open-source-parsers-jsoncpp/defaults.h>

#include <chrono>
#include <stdexcept>

std::string& JwtService::secret()
{
    static std::string value = "dev-only-change-me";
    return value;
}

int& JwtService::expires()
{
    static int value = 3600;
    return value;
}

void JwtService::configure(std::string s, int expiresSeconds)
{
    if (s.empty())
    {
        throw std::invalid_argument("jwt_secret must not be empty");
    }
    secret() = std::move(s);
    expires() = expiresSeconds > 0 ? expiresSeconds : 3600;
}

int JwtService::expiresSeconds()
{
    return expires();
}

std::string JwtService::createToken(int32_t userId, const std::string& email)
{
    const auto now = std::chrono::system_clock::now();
    return jwt::create()
        .set_issuer("url-shortener")
        .set_type("JWT")
        .set_issued_at(now)
        .set_expires_at(now + std::chrono::seconds{expires()})
        .set_subject(std::to_string(userId))
        .set_payload_claim("email", jwt::claim(email))
        .sign(jwt::algorithm::hs256{secret()});
}

std::optional<JwtClaims> JwtService::verifyToken(const std::string& token)
{
    try
    {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::hs256{secret()})
                            .with_issuer("url-shortener");
        verifier.verify(decoded);

        JwtClaims claims;
        claims.userId = std::stoi(decoded.get_subject());
        claims.email = decoded.get_payload_claim("email").as_string();
        if (claims.userId <= 0)
        {
            return std::nullopt;
        }
        return claims;
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

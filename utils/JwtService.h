#pragma once

#include <cstdint>
#include <optional>
#include <string>

struct JwtClaims
{
    int32_t userId{0};
    std::string email;
};

class JwtService
{
  public:
    static void configure(std::string secret, int expiresSeconds);

    static std::string createToken(int32_t userId, const std::string& email);

    static std::optional<JwtClaims> verifyToken(const std::string& token);

    static int expiresSeconds();

  private:
    static std::string& secret();
    static int& expires();
};

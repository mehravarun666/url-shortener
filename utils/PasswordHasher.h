#pragma once

#include <string>

class PasswordHasher
{
  public:
    /// Returns "iterations$salt_b64$hash_b64" suitable for storage.
    static std::string hash(const std::string& password);

    /// Verifies a password against a stored hash from hash().
    static bool verify(const std::string& password, const std::string& stored);
};

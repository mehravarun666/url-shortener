#include "PasswordHasher.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <stdexcept>
#include <vector>

namespace
{
constexpr int kIterations = 100000;
constexpr int kSaltLen = 16;
constexpr int kKeyLen = 32;

std::string b64Encode(const unsigned char* data, size_t len)
{
    static const char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3)
    {
        const unsigned int n = (static_cast<unsigned int>(data[i]) << 16) |
                               ((i + 1 < len ? data[i + 1] : 0) << 8) |
                               (i + 2 < len ? data[i + 2] : 0);
        out.push_back(kTable[(n >> 18) & 63]);
        out.push_back(kTable[(n >> 12) & 63]);
        out.push_back(i + 1 < len ? kTable[(n >> 6) & 63] : '=');
        out.push_back(i + 2 < len ? kTable[n & 63] : '=');
    }
    return out;
}

std::vector<unsigned char> b64Decode(const std::string& in)
{
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z')
            return c - 'A';
        if (c >= 'a' && c <= 'z')
            return c - 'a' + 26;
        if (c >= '0' && c <= '9')
            return c - '0' + 52;
        if (c == '+')
            return 62;
        if (c == '/')
            return 63;
        return -1;
    };

    std::vector<unsigned char> out;
    out.reserve(in.size() * 3 / 4);
    int valb = 0;
    int valbBits = -8;
    for (char c : in)
    {
        if (c == '=')
            break;
        const int d = val(c);
        if (d < 0)
            continue;
        valb = (valb << 6) + d;
        valbBits += 6;
        if (valbBits >= 0)
        {
            out.push_back(static_cast<unsigned char>((valb >> valbBits) & 0xFF));
            valbBits -= 8;
        }
    }
    return out;
}

std::vector<unsigned char> derive(const std::string& password,
                                  const unsigned char* salt,
                                  int saltLen,
                                  int iterations)
{
    std::vector<unsigned char> key(kKeyLen);
    if (PKCS5_PBKDF2_HMAC(password.c_str(),
                          static_cast<int>(password.size()),
                          salt,
                          saltLen,
                          iterations,
                          EVP_sha256(),
                          kKeyLen,
                          key.data()) != 1)
    {
        throw std::runtime_error("PBKDF2 failed");
    }
    return key;
}
}  // namespace

std::string PasswordHasher::hash(const std::string& password)
{
    unsigned char salt[kSaltLen];
    if (RAND_bytes(salt, kSaltLen) != 1)
    {
        throw std::runtime_error("Failed to generate salt");
    }
    const auto key = derive(password, salt, kSaltLen, kIterations);
    return std::to_string(kIterations) + "$" + b64Encode(salt, kSaltLen) + "$" +
           b64Encode(key.data(), key.size());
}

bool PasswordHasher::verify(const std::string& password, const std::string& stored)
{
    const auto first = stored.find('$');
    const auto second = stored.find('$', first == std::string::npos ? 0 : first + 1);
    if (first == std::string::npos || second == std::string::npos)
    {
        return false;
    }
    try
    {
        const int iterations = std::stoi(stored.substr(0, first));
        const auto salt = b64Decode(stored.substr(first + 1, second - first - 1));
        const auto expected = b64Decode(stored.substr(second + 1));
        if (salt.empty() || expected.size() != static_cast<size_t>(kKeyLen))
        {
            return false;
        }
        const auto actual =
            derive(password, salt.data(), static_cast<int>(salt.size()), iterations);
        if (actual.size() != expected.size())
        {
            return false;
        }
        unsigned char diff = 0;
        for (size_t i = 0; i < actual.size(); ++i)
        {
            diff |= static_cast<unsigned char>(actual[i] ^ expected[i]);
        }
        return diff == 0;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

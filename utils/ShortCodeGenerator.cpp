#include "ShortCodeGenerator.h"

#include <random>
#include <string>

namespace
{
constexpr char kAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
constexpr std::size_t kAlphabetSize = sizeof(kAlphabet) - 1;
}

std::string ShortCodeGenerator::generate()
{
    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist(0, kAlphabetSize - 1);

    std::string code;
    code.reserve(kCodeLength);
    for (std::size_t i = 0; i < kCodeLength; ++i)
    {
        code.push_back(kAlphabet[dist(rng)]);
    }
    return code;
}

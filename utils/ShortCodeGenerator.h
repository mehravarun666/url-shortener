#pragma once

#include <string>

class ShortCodeGenerator
{
  public:
    static constexpr std::size_t kCodeLength = 6;

    /// Generates a random 6-character code using A-Z, a-z, 0-9.
    static std::string generate();
};

#pragma once

#include <stdexcept>
#include <string>

#include "pinggen/token.hpp"

namespace pinggen {

class CompileError : public std::runtime_error {
  public:
    CompileError(const SourceLocation& location, const std::string& message);
    const SourceLocation& location() const noexcept { return location_; }

  private:
    SourceLocation location_;
};

[[noreturn]] void fail(const SourceLocation& location, const std::string& message);

}  // namespace pinggen

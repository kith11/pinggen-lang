#include "pinggen/diagnostics.hpp"

#include <sstream>

namespace pinggen {

CompileError::CompileError(const SourceLocation& location, const std::string& message)
    : std::runtime_error([&]() {
          std::ostringstream out;
          out << "line " << location.line << ", column " << location.column << ": " << message;
          return out.str();
      }()),
      location_(location) {}

[[noreturn]] void fail(const SourceLocation& location, const std::string& message) {
    throw CompileError(location, message);
}

}  // namespace pinggen

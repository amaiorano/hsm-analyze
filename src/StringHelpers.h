#pragma once

#include <cstdarg>

// Utility for creating a temporary formatted string
template <int MaxLength = 1024> class FormatString {
  char _buffer[MaxLength];

public:
  FormatString(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int Result = vsnprintf(_buffer, MaxLength, format, args);
    // If buffer too short, make sure to null terminate it
    if (Result < 0 || Result >= MaxLength) {
      _buffer[MaxLength - 1] = '\0';
    }
    va_end(args);
  }
  const char *value() const { return _buffer; }
  operator const char *() const { return value(); }
};

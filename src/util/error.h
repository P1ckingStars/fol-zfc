#pragma once

#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <variant>

namespace logic::util {

// Source location information
struct SourceLocation {
    const char* file;
    int line;

    constexpr SourceLocation(const char* f = __builtin_FILE(),
                             int l = __builtin_LINE())
        : file(f), line(l) {}
};

// Extract filename from path
constexpr const char* filename_from_path(const char* path) {
    const char* file = path;
    while (*path) {
        if (*path == '/' || *path == '\\') {
            file = path + 1;
        }
        ++path;
    }
    return file;
}

// Error type with source location and message
class Error {
public:
    Error(std::string message, const char* file, int line)
        : message_(std::move(message)), file_(file), line_(line) {}

    const std::string& message() const { return message_; }
    const char* file() const { return file_; }
    int line() const { return line_; }

    // Convert to string for logging
    std::string to_string() const {
        std::ostringstream ss;
        ss << filename_from_path(file_) << ":" << line_ << ": " << message_;
        return ss.str();
    }

    // Stream output
    friend std::ostream& operator<<(std::ostream& os, const Error& err) {
        return os << err.to_string();
    }

private:
    std::string message_;
    const char* file_;
    int line_;
};

// Builder class for constructing Error with << operator
class ErrorBuilder {
public:
    ErrorBuilder(const char* file, int line)
        : file_(file), line_(line) {}

    template<typename T>
    ErrorBuilder& operator<<(const T& value) {
        ss_ << value;
        return *this;
    }

    // Implicit conversion to Error
    operator Error() const {
        return Error(ss_.str(), file_, line_);
    }

private:
    const char* file_;
    int line_;
    std::ostringstream ss_;
};

// Macro to create Error with current source location (supports << chaining)
#define MAKE_ERROR ::logic::util::ErrorBuilder(__FILE__, __LINE__)

// Result type: either a value T or an Error
template<typename T>
class Result {
public:
    // Success constructors
    Result(T value) : data_(std::move(value)) {}

    // Error constructor
    Result(Error error) : data_(std::move(error)) {}

    bool ok() const { return std::holds_alternative<T>(data_); }
    bool is_error() const { return std::holds_alternative<Error>(data_); }

    explicit operator bool() const { return ok(); }

    // Access value (call only if ok())
    T& value() { return std::get<T>(data_); }
    const T& value() const { return std::get<T>(data_); }

    // Access error (call only if is_error())
    Error& error() { return std::get<Error>(data_); }
    const Error& error() const { return std::get<Error>(data_); }

    // Unwrap with default value
    T value_or(T default_val) const {
        return ok() ? std::get<T>(data_) : std::move(default_val);
    }

    // Move value out (call only if ok())
    T unwrap() && {
        return std::move(std::get<T>(data_));
    }

private:
    std::variant<T, Error> data_;
};

// Specialization for void return type
template<>
class Result<void> {
public:
    Result() : error_(std::nullopt) {}
    Result(Error error) : error_(std::move(error)) {}

    bool ok() const { return !error_.has_value(); }
    bool is_error() const { return error_.has_value(); }

    explicit operator bool() const { return ok(); }

    Error& error() { return *error_; }
    const Error& error() const { return *error_; }

private:
    std::optional<Error> error_;
};

// Helper to return success for Result<void>
inline Result<void> Ok() { return Result<void>(); }

using ResultStatus = Result<void>;

// Macro to propagate errors (early return if error)
#define TRY(expr) \
    do { \
        auto&& _result = (expr); \
        if (_result.is_error()) { \
            return std::move(_result).error(); \
        } \
    } while (false)

// Macro to propagate errors and get value
#define TRY_ASSIGN(var, expr) \
    auto _result_##var = (expr); \
    if (_result_##var.is_error()) { \
        return std::move(_result_##var).error(); \
    } \
    auto var = std::move(_result_##var).unwrap()

}  // namespace logic::util

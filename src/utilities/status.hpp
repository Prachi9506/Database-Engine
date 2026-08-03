#pragma once

#include <string>
#include <utility>

namespace minidb::util {

enum class Code {
  kOk = 0,
  kNotFound,
  kInvalidArgument,
  kOutOfSpace,
  kIOError,
  kCorruption,
  kAlreadyExists,
  kConflict,  
};

class [[nodiscard]] Status {
 public:
  Status() : code_(Code::kOk) {}
  static Status OK() { return Status(); }
  static Status NotFound(std::string msg) { return Status(Code::kNotFound, std::move(msg)); }
  static Status InvalidArgument(std::string msg) { return Status(Code::kInvalidArgument, std::move(msg)); }
  static Status OutOfSpace(std::string msg) { return Status(Code::kOutOfSpace, std::move(msg)); }
  static Status IOError(std::string msg) { return Status(Code::kIOError, std::move(msg)); }
  static Status Corruption(std::string msg) { return Status(Code::kCorruption, std::move(msg)); }
  static Status AlreadyExists(std::string msg) { return Status(Code::kAlreadyExists, std::move(msg)); }
  static Status Conflict(std::string msg) { return Status(Code::kConflict, std::move(msg)); }

  bool ok() const { return code_ == Code::kOk; }
  Code code() const { return code_; }
  const std::string& message() const { return message_; }

 private:
  Status(Code c, std::string msg) : code_(c), message_(std::move(msg)) {}
  Code code_;
  std::string message_;
};

template <typename T>
class [[nodiscard]] Result {
 public:
  Result(T value) : status_(Status::OK()), value_(std::move(value)), has_value_(true) {}
  Result(Status status) : status_(std::move(status)), has_value_(false) {}

  bool ok() const { return status_.ok(); }
  const Status& status() const { return status_; }

  T& value() { return value_; }
  const T& value() const { return value_; }

 private:
  Status status_;
  T value_{};
  bool has_value_;
};

} 

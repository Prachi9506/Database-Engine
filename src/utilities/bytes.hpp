#pragma once

#include <cstdint>
#include <cstring>

namespace minidb::util {

template <typename T>
inline void WriteFixed(char* dst, T value) {
  static_assert(std::is_trivially_copyable<T>::value, "WriteFixed requires a POD type");
  std::memcpy(dst, &value, sizeof(T));
}

template <typename T>
inline T ReadFixed(const char* src) {
  static_assert(std::is_trivially_copyable<T>::value, "ReadFixed requires a POD type");
  T value;
  std::memcpy(&value, src, sizeof(T));
  return value;
}

}  

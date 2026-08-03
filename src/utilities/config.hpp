#pragma once

#include <cstddef>
#include <cstdint>

namespace minidb::config {

inline constexpr std::size_t kPageSize = 4096;

inline constexpr std::size_t kDefaultBufferPoolSize = 64;

inline constexpr std::int32_t kHeaderPageId = 0;
inline constexpr std::int32_t kFirstDataPageId = 1;

}  

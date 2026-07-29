#pragma once

#include <string>
#include <utility>

#include "network/socket_compat.hpp"
#include "utilities/status.hpp"

namespace minidb::network {

inline constexpr std::size_t kMaxFrameSize = 64 * 1024 * 1024;  


util::Status WriteFrame(socket_t sock, const std::string& payload);


util::Result<std::string> ReadFrame(socket_t sock);

util::Status WriteResponse(socket_t sock, bool ok, const std::string& payload);

util::Result<std::pair<bool, std::string>> ReadResponse(socket_t sock);

}  

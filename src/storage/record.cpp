#include "storage/record.hpp"

#include <cstring>

#include "utilities/bytes.hpp"

namespace minidb::storage {

using minidb::common::ColumnType;
using minidb::common::Schema;
using minidb::common::Value;

util::Result<std::string> RecordCodec::Encode(const Schema& schema, const std::vector<Value>& values) {
  if (values.size() != schema.ColumnCount()) {
    return util::Status::InvalidArgument("RecordCodec::Encode: value count does not match schema");
  }

  std::size_t bitmap_bytes = NullBitmapBytes(schema.ColumnCount());
  std::string out(bitmap_bytes, '\0');

  for (std::size_t i = 0; i < schema.ColumnCount(); ++i) {
    const auto& col = schema.At(i);
    const Value& v = values[i];

    if (!minidb::common::ValueMatchesType(v, col.type)) {
      return util::Status::InvalidArgument("RecordCodec::Encode: type mismatch for column '" + col.name + "'");
    }

    if (minidb::common::IsNull(v)) {
      if (!col.nullable) {
        return util::Status::InvalidArgument("RecordCodec::Encode: NULL given for NOT NULL column '" + col.name + "'");
      }
      out[i / 8] = static_cast<char>(out[i / 8] | (1 << (i % 8)));
      continue;
    }

    switch (col.type) {
      case ColumnType::kInt: {
        std::int32_t iv = std::get<std::int32_t>(v);
        char buf[4];
        util::WriteFixed<std::int32_t>(buf, iv);
        out.append(buf, 4);
        break;
      }
      case ColumnType::kBigInt: {
        std::int64_t iv = std::get<std::int64_t>(v);
        char buf[8];
        util::WriteFixed<std::int64_t>(buf, iv);
        out.append(buf, 8);
        break;
      }
      case ColumnType::kDouble: {
        double dv = std::get<double>(v);
        char buf[8];
        util::WriteFixed<double>(buf, dv);
        out.append(buf, 8);
        break;
      }
      case ColumnType::kBool: {
        bool bv = std::get<bool>(v);
        char buf[1] = {static_cast<char>(bv ? 1 : 0)};
        out.append(buf, 1);
        break;
      }
      case ColumnType::kVarchar: {
        const std::string& sv = std::get<std::string>(v);
        if (sv.size() > col.max_length) {
          return util::Status::InvalidArgument("RecordCodec::Encode: value too long for column '" + col.name + "'");
        }
        if (sv.size() > 0xFFFF) {
          return util::Status::InvalidArgument("RecordCodec::Encode: value exceeds 65535-byte codec limit");
        }
        char len_buf[2];
        util::WriteFixed<std::uint16_t>(len_buf, static_cast<std::uint16_t>(sv.size()));
        out.append(len_buf, 2);
        out.append(sv);
        break;
      }
    }
  }

  return util::Result<std::string>(std::move(out));
}

util::Result<std::vector<Value>> RecordCodec::Decode(const Schema& schema, const std::string& bytes) {
  std::size_t bitmap_bytes = NullBitmapBytes(schema.ColumnCount());
  if (bytes.size() < bitmap_bytes) {
    return util::Status::Corruption("RecordCodec::Decode: record shorter than null bitmap");
  }

  std::vector<Value> values;
  values.reserve(schema.ColumnCount());

  std::size_t cursor = bitmap_bytes;
  for (std::size_t i = 0; i < schema.ColumnCount(); ++i) {
    const auto& col = schema.At(i);
    bool is_null = (bytes[i / 8] & (1 << (i % 8))) != 0;
    if (is_null) {
      values.emplace_back(std::monostate{});
      continue;
    }

    switch (col.type) {
      case ColumnType::kInt: {
        if (cursor + 4 > bytes.size()) return util::Status::Corruption("RecordCodec::Decode: truncated INT");
        values.emplace_back(util::ReadFixed<std::int32_t>(bytes.data() + cursor));
        cursor += 4;
        break;
      }
      case ColumnType::kBigInt: {
        if (cursor + 8 > bytes.size()) return util::Status::Corruption("RecordCodec::Decode: truncated BIGINT");
        values.emplace_back(util::ReadFixed<std::int64_t>(bytes.data() + cursor));
        cursor += 8;
        break;
      }
      case ColumnType::kDouble: {
        if (cursor + 8 > bytes.size()) return util::Status::Corruption("RecordCodec::Decode: truncated DOUBLE");
        values.emplace_back(util::ReadFixed<double>(bytes.data() + cursor));
        cursor += 8;
        break;
      }
      case ColumnType::kBool: {
        if (cursor + 1 > bytes.size()) return util::Status::Corruption("RecordCodec::Decode: truncated BOOLEAN");
        values.emplace_back(bytes[cursor] != 0);
        cursor += 1;
        break;
      }
      case ColumnType::kVarchar: {
        if (cursor + 2 > bytes.size()) return util::Status::Corruption("RecordCodec::Decode: truncated VARCHAR length");
        std::uint16_t len = util::ReadFixed<std::uint16_t>(bytes.data() + cursor);
        cursor += 2;
        if (cursor + len > bytes.size()) return util::Status::Corruption("RecordCodec::Decode: truncated VARCHAR payload");
        values.emplace_back(std::string(bytes.data() + cursor, len));
        cursor += len;
        break;
      }
    }
  }

  return util::Result<std::vector<Value>>(std::move(values));
}

}  

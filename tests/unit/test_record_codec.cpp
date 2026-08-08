#include "storage/record.hpp"

#include "test_framework.hpp"

using namespace minidb::common;
using namespace minidb::storage;

namespace {
Schema MakeUserSchema() {
  return Schema({
      Column{"id", ColumnType::kInt, false, 0},
      Column{"name", ColumnType::kVarchar, true, 50},
      Column{"balance", ColumnType::kDouble, true, 0},
      Column{"active", ColumnType::kBool, true, 0},
      Column{"big", ColumnType::kBigInt, true, 0},
  });
}
}  

TEST(RecordCodec, EncodeDecodeRoundTrip) {
  Schema schema = MakeUserSchema();
  std::vector<Value> values = {std::int32_t(42), std::string("alice"), 99.5, true, std::int64_t(123456789012)};

  auto encoded = RecordCodec::Encode(schema, values);
  EXPECT_TRUE(encoded.ok());

  auto decoded = RecordCodec::Decode(schema, encoded.value());
  EXPECT_TRUE(decoded.ok());
  EXPECT_EQ(decoded.value().size(), values.size());
  EXPECT_EQ(std::get<std::int32_t>(decoded.value()[0]), 42);
  EXPECT_EQ(std::get<std::string>(decoded.value()[1]), "alice");
  EXPECT_EQ(std::get<double>(decoded.value()[2]), 99.5);
  EXPECT_EQ(std::get<bool>(decoded.value()[3]), true);
  EXPECT_EQ(std::get<std::int64_t>(decoded.value()[4]), 123456789012LL);
}

TEST(RecordCodec, NullValuesRoundTripAndCostNoPayloadBytes) {
  Schema schema = MakeUserSchema();
  std::vector<Value> all_null = {std::int32_t(1), std::monostate{}, std::monostate{}, std::monostate{},
                                  std::monostate{}};
  std::vector<Value> some_data = {std::int32_t(1), std::string("this is a long-ish name"), 3.14, false,
                                   std::int64_t(1)};

  auto encoded_null = RecordCodec::Encode(schema, all_null);
  auto encoded_data = RecordCodec::Encode(schema, some_data);
  EXPECT_TRUE(encoded_null.ok());
  EXPECT_TRUE(encoded_data.ok());

  EXPECT_TRUE(encoded_null.value().size() < encoded_data.value().size());

  auto decoded = RecordCodec::Decode(schema, encoded_null.value());
  EXPECT_TRUE(decoded.ok());
  EXPECT_TRUE(IsNull(decoded.value()[1]));
  EXPECT_TRUE(IsNull(decoded.value()[2]));
  EXPECT_TRUE(IsNull(decoded.value()[3]));
  EXPECT_TRUE(IsNull(decoded.value()[4]));
  EXPECT_FALSE(IsNull(decoded.value()[0]));
}

TEST(RecordCodec, NullForNotNullColumnRejected) {
  Schema schema = MakeUserSchema();  
  std::vector<Value> values = {std::monostate{}, std::string("x"), 1.0, true, std::int64_t(1)};
  auto encoded = RecordCodec::Encode(schema, values);
  EXPECT_FALSE(encoded.ok());
}

TEST(RecordCodec, TypeMismatchRejected) {
  Schema schema = MakeUserSchema();
  std::vector<Value> values = {std::string("not-an-int"), std::string("x"), 1.0, true, std::int64_t(1)};
  auto encoded = RecordCodec::Encode(schema, values);
  EXPECT_FALSE(encoded.ok());
}

TEST(RecordCodec, VarcharExceedingMaxLengthRejected) {
  Schema schema = MakeUserSchema();  
  std::string too_long(51, 'a');
  std::vector<Value> values = {std::int32_t(1), too_long, 1.0, true, std::int64_t(1)};
  auto encoded = RecordCodec::Encode(schema, values);
  EXPECT_FALSE(encoded.ok());
}

TEST(RecordCodec, WrongValueCountRejected) {
  Schema schema = MakeUserSchema();
  std::vector<Value> values = {std::int32_t(1), std::string("x")};  
  auto encoded = RecordCodec::Encode(schema, values);
  EXPECT_FALSE(encoded.ok());
}

#include "bundled/doctest.h"

#include <string>

#include "bitlog/backend/decode.h"
#include "bitlog/bundled/fmt/format.h"
#include "bitlog/frontend/encode.h"

using namespace bitlog;
using namespace bitlog::detail;

TEST_SUITE_BEGIN("EncodeDecode");

TEST_CASE("get_type_descriptor")
{
  // Fundamental Types
  REQUIRE_EQ(GetTypeDescriptor<char>::value, TypeDescriptorName::Char);
  REQUIRE_EQ(GetTypeDescriptor<unsigned char>::value, TypeDescriptorName::UnsignedChar);
  REQUIRE_EQ(GetTypeDescriptor<short int>::value, TypeDescriptorName::ShortInt);
  REQUIRE_EQ(GetTypeDescriptor<unsigned short int>::value, TypeDescriptorName::UnsignedShortInt);
  REQUIRE_EQ(GetTypeDescriptor<int>::value, TypeDescriptorName::Int);
  REQUIRE_EQ(GetTypeDescriptor<unsigned int>::value, TypeDescriptorName::UnsignedInt);
  REQUIRE_EQ(GetTypeDescriptor<long int>::value, TypeDescriptorName::LongInt);
  REQUIRE_EQ(GetTypeDescriptor<unsigned long int>::value, TypeDescriptorName::UnsignedLongInt);
  REQUIRE_EQ(GetTypeDescriptor<long long int>::value, TypeDescriptorName::LongLongInt);
  REQUIRE_EQ(GetTypeDescriptor<unsigned long long int>::value, TypeDescriptorName::UnsignedLongLongInt);
  REQUIRE_EQ(GetTypeDescriptor<float>::value, TypeDescriptorName::Float);
  REQUIRE_EQ(GetTypeDescriptor<double>::value, TypeDescriptorName::Double);

  // Pointers
  REQUIRE_EQ(GetTypeDescriptor<char const*>::value, TypeDescriptorName::CString);
  REQUIRE_EQ(GetTypeDescriptor<char*>::value, TypeDescriptorName::CString);

  // Arrays
  REQUIRE_EQ(GetTypeDescriptor<char[10]>::value, TypeDescriptorName::CStringArray);
  REQUIRE_EQ(GetTypeDescriptor<char const[20]>::value, TypeDescriptorName::CStringArray);
  REQUIRE_EQ(GetTypeDescriptor<char(&)[13]>::value, TypeDescriptorName::CStringArray);
  REQUIRE_EQ(GetTypeDescriptor<char const(&)[13]>::value, TypeDescriptorName::CStringArray);

  // References
  REQUIRE_EQ(GetTypeDescriptor<char&>::value, TypeDescriptorName::Char);
  REQUIRE_EQ(GetTypeDescriptor<char const&>::value, TypeDescriptorName::Char);
  REQUIRE_EQ(GetTypeDescriptor<char*&>::value, TypeDescriptorName::CString);
  REQUIRE_EQ(GetTypeDescriptor<char const*&>::value, TypeDescriptorName::CString);
  REQUIRE_EQ(GetTypeDescriptor<char const* const&>::value, TypeDescriptorName::CString);

  // Strings
  REQUIRE_EQ(GetTypeDescriptor<std::string>::value, TypeDescriptorName::StdString);
  REQUIRE_EQ(GetTypeDescriptor<std::string_view>::value, TypeDescriptorName::StdString);
}

TEST_CASE("is_c_style_string_type")
{
  char const* s1 = "const_char_pointer";
  REQUIRE(is_c_style_string_type<decltype(s1)>());
  REQUIRE_EQ(GetTypeDescriptor<decltype(s1)>::value, TypeDescriptorName::CString);

  std::string s2 = "string";
  REQUIRE_FALSE(is_c_style_string_type<decltype(s2)>());
  REQUIRE_EQ(GetTypeDescriptor<decltype(s2)>::value, TypeDescriptorName::StdString);

  char* s3 = s2.data();
  REQUIRE(is_c_style_string_type<decltype(s3)>());
  REQUIRE_EQ(GetTypeDescriptor<decltype(s3)>::value, TypeDescriptorName::CString);

  char s4[] = "mutable_array";
  REQUIRE(is_c_style_string_type<decltype(s4)>());
  REQUIRE_EQ(GetTypeDescriptor<decltype(s4)>::value, TypeDescriptorName::CStringArray);

  char const s5[] = "const_char_array";
  REQUIRE(is_c_style_string_type<decltype(s5)>());
  REQUIRE_EQ(GetTypeDescriptor<decltype(s5)>::value, TypeDescriptorName::CStringArray);

  const char s6[] = "const_array_ref";
  const char(&ref)[sizeof(s6)] = s6;
  REQUIRE(is_c_style_string_type<decltype(ref)>());
  REQUIRE_EQ(GetTypeDescriptor<decltype(ref)>::value, TypeDescriptorName::CStringArray);

  REQUIRE(is_c_style_string_type<decltype("string_literal")>());
  REQUIRE_EQ(GetTypeDescriptor<decltype("string_literal")>::value, TypeDescriptorName::CStringArray);

  REQUIRE_FALSE(is_c_style_string_type<void*>());
  REQUIRE_FALSE(is_c_style_string_type<void>());
  REQUIRE_FALSE(is_c_style_string_type<char>());
}

TEST_CASE("is_std_string_type")
{
  std::string s1 = "string";
  REQUIRE(is_std_string_type<decltype(s1)>());

  std::string_view sv1 = "string_view";
  REQUIRE(is_std_string_type<decltype(sv1)>());

  const std::string s2 = "const_string";
  REQUIRE(is_std_string_type<decltype(s2)>());

  std::string* ps3 = nullptr;
  REQUIRE_FALSE(is_std_string_type<decltype(ps3)>());

  std::string_view* psv4 = nullptr;
  REQUIRE_FALSE(is_std_string_type<decltype(psv4)>());

  REQUIRE_FALSE(is_std_string_type<void*>());
  REQUIRE_FALSE(is_std_string_type<void>());
  REQUIRE_FALSE(is_std_string_type<char>());

  char const arr[] = "const_char_array";
  REQUIRE_FALSE(is_std_string_type<decltype(arr)>());

  char const* ptr = arr;
  REQUIRE_FALSE(is_std_string_type<decltype(ptr)>());
}

TEST_CASE("is_char_array_type")
{
  char s1[] = "mutable_array";
  REQUIRE(is_char_array_type<decltype(s1)>());

  const char s2[] = "const_array";
  REQUIRE(is_char_array_type<decltype(s2)>());

  char const s3[] = "const_char_array";
  REQUIRE(is_char_array_type<decltype(s3)>());

  const char s4[] = "const_array_ref";
  const char(&ref)[sizeof(s4)] = s4;
  REQUIRE(is_char_array_type<decltype(ref)>());

  int arr[5] = {1, 2, 3, 4, 5};
  REQUIRE_FALSE(is_char_array_type<decltype(arr)>());

  int* ptr = arr;
  REQUIRE_FALSE(is_char_array_type<decltype(ptr)>());

  std::string str = "not_char_array";
  REQUIRE_FALSE(is_char_array_type<decltype(str)>());

  REQUIRE_FALSE(is_char_array_type<void*>());
  REQUIRE_FALSE(is_char_array_type<void>());
  REQUIRE_FALSE(is_char_array_type<char>());
}

TEST_CASE("count_c_style_strings")
{
  char const* s1 = "string";
  char* s2 = nullptr;
  std::string s3 = "not_c_style_string";
  const char s4[] = "const_array";
  REQUIRE_EQ(count_c_style_strings<decltype(s1), decltype(s2), decltype(s3), decltype(s4)>(), 3);

  char s5[] = "mutable_array";
  const char s6[] = "const_array_ref";
  const char(&ref)[sizeof(s6)] = s6;
  REQUIRE_EQ(count_c_style_strings<decltype(s5), decltype(s6), decltype(ref)>(), 3);

  int arr[5] = {1, 2, 3, 4, 5};
  REQUIRE_EQ(count_c_style_strings<decltype(arr)>(), 0);

  int* ptr = arr;
  REQUIRE_EQ(count_c_style_strings<decltype(ptr)>(), 0);

  REQUIRE_EQ(count_c_style_strings<>(), 0);
  REQUIRE_EQ(count_c_style_strings<void*>(), 0);
  REQUIRE_EQ(count_c_style_strings<void>(), 0);
  REQUIRE_EQ(count_c_style_strings<char>(), 0);
  REQUIRE_EQ(count_c_style_strings<char, decltype("literal")>(), 1);
}

TEST_CASE("calculate_args_size_and_populate_string_lengths_1")
{
  char const* s1 = "mutable_array";
  const char s2[] = "const_array_ref";
  const char(&ref)[sizeof(s2)] = s2;

  uint32_t c_style_string_lengths[4];
  uint32_t total_size = calculate_args_size_and_populate_string_lengths(
    c_style_string_lengths, s1, s2, ref, (int)42, "literal", (double)3.14, std::string("std_string"));

  REQUIRE_EQ(total_size,
             (strlen(s1) + 1 + sizeof(s2) + sizeof(uint32_t) - 1 + sizeof(ref) + sizeof(uint32_t) - 1 +
              sizeof(int) + sizeof("literal") + sizeof(uint32_t) - 1 + sizeof(double) + sizeof(uint32_t) + 10));

  REQUIRE_EQ(c_style_string_lengths[0], strlen(s1) + 1);
  REQUIRE_EQ(c_style_string_lengths[1], sizeof(s2) - 1);
  REQUIRE_EQ(c_style_string_lengths[2], sizeof(ref) - 1);
  REQUIRE_EQ(c_style_string_lengths[3], 7);
}

TEST_CASE("calculate_args_size_and_populate_string_lengths_2")
{
  const char s4[] = "const_array";
  const char s5[] = "another_literal";
  std::string s6 = "string_object";

  uint32_t c_style_string_lengths[3];
  uint32_t total_size = calculate_args_size_and_populate_string_lengths(
    c_style_string_lengths, s4, s5, (int)42, "literal", (double)3.14, std::string("std_string"), s6);

  REQUIRE_EQ(total_size,
             sizeof(s4) + sizeof(uint32_t) - 1 + sizeof(s5) + sizeof(uint32_t) - 1 + sizeof(int) +
               sizeof("literal") + sizeof(uint32_t) - 1 + sizeof(double) + sizeof(uint32_t) + 10 +
               sizeof(uint32_t) + s6.size());

  REQUIRE_EQ(c_style_string_lengths[0], sizeof(s4) - 1);
  REQUIRE_EQ(c_style_string_lengths[1], sizeof(s5) - 1);
}

TEST_CASE("encode_1")
{
  uint8_t buffer[256];

  // Test case with different argument types
  char s1[] = "mutable_array";
  const char s2[] = "const_array_ref";
  const char(&ref)[sizeof(s2)] = s2;
  const char* s3 = "const_char_pointer";

  uint32_t c_style_string_lengths[4];
  uint32_t total_size = calculate_args_size_and_populate_string_lengths(
    c_style_string_lengths, s1, s2, ref, s3, (int)42, "literal", (double)3.14, std::string("std_string"));

  REQUIRE_EQ(total_size,
             sizeof(s1) + sizeof(uint32_t) - 1 + sizeof(s2) + sizeof(uint32_t) - 1 + sizeof(ref) +
               sizeof(uint32_t) - 1 + strlen(s3) + 1 + sizeof(int) + sizeof("literal") +
               sizeof(uint32_t) - 1 + sizeof(double) + sizeof(uint32_t) + 10);

  uint8_t* buffer_ptr = buffer;
  encode<false>(buffer_ptr, c_style_string_lengths, s1, s2, ref, s3, (int)42, "literal", (double)3.14, std::string("std_string"));

  REQUIRE_EQ(buffer_ptr - buffer, total_size);

  // Decode the values and check if they match the original values
  uint8_t* buffer_ptr_2 = buffer;
  char s1_decoded[14];
  char s2_decoded[16];
  char ref_decoded[16];
  char s3_decoded[18];
  int int_decoded = 0;
  char literal_decoded[8];
  double double_decoded = 0.0;
  char string_decoded[11];

  // Decode s1
  uint32_t len_s1;
  std::memcpy(&len_s1, buffer_ptr_2, sizeof(len_s1));
  buffer_ptr_2 += sizeof(len_s1);

  std::memcpy(s1_decoded, buffer_ptr_2, len_s1);
  s1_decoded[len_s1] = '\0';
  buffer_ptr_2 += len_s1;

  // Decode s2
  uint32_t len_s2;
  std::memcpy(&len_s2, buffer_ptr_2, sizeof(len_s2));
  buffer_ptr_2 += sizeof(len_s2);

  std::memcpy(s2_decoded, buffer_ptr_2, len_s2);
  s2_decoded[len_s2] = '\0';
  buffer_ptr_2 += len_s2;

  // Decode ref
  uint32_t len_ref;
  std::memcpy(&len_ref, buffer_ptr_2, sizeof(len_ref));
  buffer_ptr_2 += sizeof(len_ref);

  std::memcpy(ref_decoded, buffer_ptr_2, len_ref);
  ref_decoded[len_ref] = '\0';
  buffer_ptr_2 += len_ref;

  // Decode s3
  uint32_t len_s3 = strlen(reinterpret_cast<char const*>(buffer_ptr_2));
  std::memcpy(s3_decoded, buffer_ptr_2, len_s3 + 1);
  buffer_ptr_2 += len_s3 + 1;

  // Decode int
  std::memcpy(&int_decoded, buffer_ptr_2, sizeof(int));
  buffer_ptr_2 += sizeof(int);

  // Decode literal
  uint32_t len_literal;
  std::memcpy(&len_literal, buffer_ptr_2, sizeof(len_literal));
  buffer_ptr_2 += sizeof(len_literal);

  std::memcpy(literal_decoded, buffer_ptr_2, len_literal);
  literal_decoded[len_literal] = '\0';
  buffer_ptr_2 += len_literal;

  // Decode double
  std::memcpy(&double_decoded, buffer_ptr_2, sizeof(double));
  buffer_ptr_2 += sizeof(double);

  // Decode std_string
  uint32_t len_std_string;
  std::memcpy(&len_std_string, buffer_ptr_2, sizeof(len_std_string));
  buffer_ptr_2 += sizeof(len_std_string);

  std::memcpy(string_decoded, buffer_ptr_2, len_std_string);
  string_decoded[len_std_string] = '\0';
  buffer_ptr_2 += len_std_string;

  // Check if decoded values match the original values
  REQUIRE_EQ(std::strcmp(s1_decoded, s1), 0);
  REQUIRE_EQ(std::strcmp(s2_decoded, s2), 0);
  REQUIRE_EQ(std::strcmp(ref_decoded, s2), 0);
  REQUIRE_EQ(std::strcmp(s3_decoded, s3), 0);
  REQUIRE_EQ(int_decoded, 42);
  REQUIRE_EQ(std::strcmp(literal_decoded, "literal"), 0);
  REQUIRE_EQ(double_decoded, 3.14);
  REQUIRE_EQ(std::strcmp(string_decoded, "std_string"), 0);
}

TEST_CASE("encode_decode")
{
  uint8_t buffer[256];

  signed char s1 = 'a';
  unsigned char s2 = 'A';
  short int s3 = -12;
  unsigned short int s4 = 24;
  int s5 = -1337;
  unsigned int s6 = 256;
  long int s7 = -90000;
  unsigned long int s8 = 100000;
  long long int s9 = -210000;
  unsigned long long int s10 = 410000;
  float s11 = 3.14;
  double s12 = 72.123;
  char s13[] = "mutable_array";
  const char* s14 = "const_char_pointer";
  std::string s15 {"std_string"};
  char s16 = 'B';

  uint32_t c_style_string_lengths[2];
  uint32_t total_size = calculate_args_size_and_populate_string_lengths(
    c_style_string_lengths, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15, s16);

  REQUIRE_EQ(c_style_string_lengths[0], 13);
  REQUIRE_EQ(c_style_string_lengths[1], 19);

  REQUIRE_EQ(total_size,
             sizeof(s1) + sizeof(s2) + sizeof(s3) + sizeof(s4) + sizeof(s5) + sizeof(s6) +
               sizeof(s7) + sizeof(s8) + sizeof(s9) + sizeof(s10) + sizeof(s11) + sizeof(s12) +
               sizeof (s13) + sizeof (uint32_t) - 1 + strlen(s14) + 1 + s15.size() + sizeof (uint32_t) + sizeof (s16));

  uint8_t* encode_buffer = buffer;
  encode<false>(encode_buffer, c_style_string_lengths, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                s12, s13, s14, s15, s16);

  REQUIRE_EQ(encode_buffer - buffer, total_size);

  std::vector<TypeDescriptorName> type_descriptors;
  type_descriptors.emplace_back(GetTypeDescriptor<decltype(s1)>::value);
  type_descriptors.emplace_back(GetTypeDescriptor<decltype(s2)>::value);
  type_descriptors.emplace_back(GetTypeDescriptor<decltype(s3)>::value);
  type_descriptors.emplace_back(GetTypeDescriptor<decltype(s4)>::value);
  type_descriptors.emplace_back(GetTypeDescriptor<decltype(s5)>::value);
  type_descriptors.emplace_back(GetTypeDescriptor<decltype(s6)>::value);
  type_descriptors.emplace_back(GetTypeDescriptor<decltype(s7)>::value);
  type_descriptors.emplace_back(GetTypeDescriptor<decltype(s8)>::value);
  type_descriptors.emplace_back(GetTypeDescriptor<decltype(s9)>::value);
  type_descriptors.emplace_back(GetTypeDescriptor<decltype(s10)>::value);
  type_descriptors.emplace_back(GetTypeDescriptor<decltype(s11)>::value);
  type_descriptors.emplace_back(GetTypeDescriptor<decltype(s12)>::value);
  type_descriptors.emplace_back(GetTypeDescriptor<decltype(s13)>::value);
  type_descriptors.emplace_back(GetTypeDescriptor<decltype(s14)>::value);
  type_descriptors.emplace_back(GetTypeDescriptor<decltype(s15)>::value);
  type_descriptors.emplace_back(GetTypeDescriptor<decltype(s16)>::value);

  // Decode
  uint8_t const* decode_buffer = buffer;
  std::vector<fmtbitlog::basic_format_arg<fmtbitlog::format_context>> fmt_args;
  decode(decode_buffer, type_descriptors, fmt_args);

  REQUIRE_EQ(fmt_args.size(), 16);

  std::string output_buffer;
  fmtbitlog::vformat_to(std::back_inserter(output_buffer), "{} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}",
                        fmtbitlog::basic_format_args(fmt_args.data(), fmt_args.size()));

  std::string_view const output {output_buffer.data(), output_buffer.size()};
  std::string_view const expected_output {"97 65 -12 24 -1337 256 -90000 100000 -210000 410000 3.14 72.123 mutable_array const_char_pointer std_string B"};

  REQUIRE_EQ(output, expected_output);
}

TEST_SUITE_END();
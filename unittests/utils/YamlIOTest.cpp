// This source file is part of the polarphp.org open source project
//
// Copyright (c) 2017 - 2019 polarphp software foundation
// Copyright (c) 2017 - 2019 zzu_softboy <zzu_softboy@163.com>
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://polarphp.org/LICENSE.txt for license information
// See https://polarphp.org/CONTRIBUTORS.txt for the list of polarphp project authors
//
// Created by polarboy on 2018/07/15.

#include "polarphp/basic/adt/StringMap.h"
#include "polarphp/basic/adt/StringRef.h"
#include "polarphp/basic/adt/Twine.h"
#include "polarphp/utils/Casting.h"
#include "polarphp/utils/Endian.h"
#include "polarphp/utils/Format.h"
#include "polarphp/utils/yaml/YamlTraits.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace polar;
using namespace polar::utils;
using namespace polar::basic;

using polar::yaml::Hex8;
using polar::yaml::Hex16;
using polar::yaml::Hex32;
using polar::yaml::Hex64;
using polar::yaml::Input;
using polar::yaml::Output;
using polar::yaml::IO;
using polar::yaml::MappingTraits;
using polar::yaml::MappingNormalization;
using polar::yaml::ScalarTraits;
using polar::yaml::is_numeric;

using ::testing::StartsWith;

static void suppress_error_messages(const SMDiagnostic &, void *) {
}

//===----------------------------------------------------------------------===//
//  test MappingTraits
//===----------------------------------------------------------------------===//

struct FooBar
{
   int foo;
   int bar;
};
typedef std::vector<FooBar> FooBarSequence;

POLAR_YAML_IS_SEQUENCE_VECTOR(FooBar)

struct FooBarContainer {
   FooBarSequence fbs;
};

namespace polar {
namespace yaml {
template <>
struct MappingTraits<FooBar>
{
   static void mapping(IO &io, FooBar& fb) {
      io.mapRequired("foo",    fb.foo);
      io.mapRequired("bar",    fb.bar);
   }
};

template <> struct MappingTraits<FooBarContainer>
{
   static void mapping(IO &io, FooBarContainer &fb) {
      io.mapRequired("fbs", fb.fbs);
   }
};
}
}


//
// test the reading of a yaml mapping
//
TEST(YamlIOTest, testMapRead)
{
   FooBar doc;
   {
      Input yin("---\nfoo:  3\nbar:  5\n...\n");
      yin >> doc;

      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(doc.foo, 3);
      EXPECT_EQ(doc.bar, 5);
   }

   {
      Input yin("{foo: 3, bar: 5}");
      yin >> doc;

      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(doc.foo, 3);
      EXPECT_EQ(doc.bar, 5);
   }
}

TEST(YamlIOTest, testMalformedMapRead)
{
   FooBar doc;
   Input yin("{foo: 3; bar: 5}", nullptr, suppress_error_messages);
   yin >> doc;
   EXPECT_TRUE(!!yin.getError());
}

//
// test the reading of a yaml sequence of mappings
//
TEST(YamlIOTest, testSequenceMapRead) {
   FooBarSequence seq;
   Input yin("---\n - foo:  3\n   bar:  5\n - foo:  7\n   bar:  9\n...\n");
   yin >> seq;

   EXPECT_FALSE(yin.getError());
   EXPECT_EQ(seq.size(), 2UL);
   FooBar& map1 = seq[0];
   FooBar& map2 = seq[1];
   EXPECT_EQ(map1.foo, 3);
   EXPECT_EQ(map1.bar, 5);
   EXPECT_EQ(map2.foo, 7);
   EXPECT_EQ(map2.bar, 9);
}

//
// test the reading of a map containing a yaml sequence of mappings
//
TEST(YamlIOTest, testContainerSequenceMapRead)
{
   {
      FooBarContainer cont;
      Input yin2("---\nfbs:\n - foo: 3\n   bar: 5\n - foo: 7\n   bar: 9\n...\n");
      yin2 >> cont;

      EXPECT_FALSE(yin2.getError());
      EXPECT_EQ(cont.fbs.size(), 2UL);
      EXPECT_EQ(cont.fbs[0].foo, 3);
      EXPECT_EQ(cont.fbs[0].bar, 5);
      EXPECT_EQ(cont.fbs[1].foo, 7);
      EXPECT_EQ(cont.fbs[1].bar, 9);
   }

   {
      FooBarContainer cont;
      Input yin("---\nfbs:\n...\n");
      yin >> cont;
      // Okay: Empty node represents an empty array.
      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(cont.fbs.size(), 0UL);
   }

   {
      FooBarContainer cont;
      Input yin("---\nfbs: !!null null\n...\n");
      yin >> cont;
      // Okay: null represents an empty array.
      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(cont.fbs.size(), 0UL);
   }

   {
      FooBarContainer cont;
      Input yin("---\nfbs: ~\n...\n");
      yin >> cont;
      // Okay: null represents an empty array.
      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(cont.fbs.size(), 0UL);
   }

   {
      FooBarContainer cont;
      Input yin("---\nfbs: null\n...\n");
      yin >> cont;
      // Okay: null represents an empty array.
      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(cont.fbs.size(), 0UL);
   }
}

//
// test the reading of a map containing a malformed yaml sequence
//
TEST(YamlIOTest, testMalformedContainerSequenceMapRead)
{
   {
      FooBarContainer cont;
      Input yin("---\nfbs:\n   foo: 3\n   bar: 5\n...\n", nullptr,
                suppress_error_messages);
      yin >> cont;
      // error: fbs is not a sequence.
      EXPECT_TRUE(!!yin.getError());
      EXPECT_EQ(cont.fbs.size(), 0UL);
   }

   {
      FooBarContainer cont;
      Input yin("---\nfbs: 'scalar'\n...\n", nullptr, suppress_error_messages);
      yin >> cont;
      // This should be an error.
      EXPECT_TRUE(!!yin.getError());
      EXPECT_EQ(cont.fbs.size(), 0UL);
   }
}

//
// test writing then reading back a sequence of mappings
//
TEST(YamlIOTest, testSequenceMapWriteAndRead)
{
   std::string intermediate;
   {
      FooBar entry1;
      entry1.foo = 10;
      entry1.bar = -3;
      FooBar entry2;
      entry2.foo = 257;
      entry2.bar = 0;
      FooBarSequence seq;
      seq.push_back(entry1);
      seq.push_back(entry2);

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << seq;
   }

   {
      Input yin(intermediate);
      FooBarSequence seq2;
      yin >> seq2;

      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(seq2.size(), 2UL);
      FooBar& map1 = seq2[0];
      FooBar& map2 = seq2[1];
      EXPECT_EQ(map1.foo, 10);
      EXPECT_EQ(map1.bar, -3);
      EXPECT_EQ(map2.foo, 257);
      EXPECT_EQ(map2.bar, 0);
   }
}

//
// test YAML filename handling.
//
static void test_error_filename(const SMDiagnostic &error, void *)
{
   EXPECT_EQ(error.getFilename(), "foo.yaml");
}

TEST(YamlIOTest, testGivenFilename)
{
   auto Buffer = MemoryBuffer::getMemBuffer("{ x: 42 }", "foo.yaml");
   Input yin(*Buffer, nullptr, test_error_filename);
   FooBar Value;
   yin >> Value;

   EXPECT_TRUE(!!yin.getError());
}


struct WithStringField {
   std::string str1;
   std::string str2;
   std::string str3;
};

namespace polar {
namespace yaml {
template <> struct MappingTraits<WithStringField> {
   static void mapping(IO &io, WithStringField &fb) {
      io.mapRequired("str1", fb.str1);
      io.mapRequired("str2", fb.str2);
      io.mapRequired("str3", fb.str3);
   }
};
} // namespace yaml
} // namespace polar

TEST(YamlIOTest, testMultilineStrings)
{
   WithStringField Original;
   Original.str1 = "a multiline string\nfoobarbaz";
   Original.str2 = "another one\rfoobarbaz";
   Original.str3 = "a one-line string";

   std::string Serialized;
   {
      RawStringOutStream OS(Serialized);
      Output YOut(OS);
      YOut << Original;
   }
   auto Expected = "---\n"
                   "str1:            'a multiline string\n"
                   "foobarbaz'\n"
                   "str2:            'another one\r"
                   "foobarbaz'\n"
                   "str3:            a one-line string\n"
                   "...\n";
   ASSERT_EQ(Serialized, Expected);

   // Also check it parses back without the errors.
   WithStringField Deserialized;
   {
      Input YIn(Serialized);
      YIn >> Deserialized;
      ASSERT_FALSE(YIn.getError())
            << "Parsing error occurred during deserialization. Serialized string:\n"
            << Serialized;
   }
   EXPECT_EQ(Original.str1, Deserialized.str1);
   EXPECT_EQ(Original.str2, Deserialized.str2);
   EXPECT_EQ(Original.str3, Deserialized.str3);
}

TEST(YamlIOTest, testNoQuotesForTab)
{
   WithStringField WithTab;
   WithTab.str1 = "aba\tcaba";
   std::string Serialized;
   {
      RawStringOutStream OS(Serialized);
      Output YOut(OS);
      YOut << WithTab;
   }
   auto ExpectedPrefix = "---\n"
                         "str1:            aba\tcaba\n";
   EXPECT_THAT(Serialized, StartsWith(ExpectedPrefix));
}

//===----------------------------------------------------------------------===//
//  test built-in types
//===----------------------------------------------------------------------===//

struct BuiltInTypes

{
   StringRef str;
   std::string stdstr;
   uint64_t        u64;
   uint32_t        u32;
   uint16_t        u16;
   uint8_t         u8;
   bool            b;
   int64_t         s64;
   int32_t         s32;
   int16_t         s16;
   int8_t          s8;
   float           f;
   double          d;
   Hex8            h8;
   Hex16           h16;
   Hex32           h32;
   Hex64           h64;
};

namespace polar {
namespace yaml {
template <>
struct MappingTraits<BuiltInTypes> {
   static void mapping(IO &io, BuiltInTypes& bt)
   {
      io.mapRequired("str",      bt.str);
      io.mapRequired("stdstr",   bt.stdstr);
      io.mapRequired("u64",      bt.u64);
      io.mapRequired("u32",      bt.u32);
      io.mapRequired("u16",      bt.u16);
      io.mapRequired("u8",       bt.u8);
      io.mapRequired("b",        bt.b);
      io.mapRequired("s64",      bt.s64);
      io.mapRequired("s32",      bt.s32);
      io.mapRequired("s16",      bt.s16);
      io.mapRequired("s8",       bt.s8);
      io.mapRequired("f",        bt.f);
      io.mapRequired("d",        bt.d);
      io.mapRequired("h8",       bt.h8);
      io.mapRequired("h16",      bt.h16);
      io.mapRequired("h32",      bt.h32);
      io.mapRequired("h64",      bt.h64);
   }
};
}
}


//
// test the reading of all built-in scalar conversions
//
TEST(YamlIOTest, testReadBuiltInTypes)
{
   BuiltInTypes map;
   Input yin("---\n"
             "str:      hello there\n"
             "stdstr:   hello where?\n"
             "u64:      5000000000\n"
             "u32:      4000000000\n"
             "u16:      65000\n"
             "u8:       255\n"
             "b:        false\n"
             "s64:      -5000000000\n"
             "s32:      -2000000000\n"
             "s16:      -32000\n"
             "s8:       -127\n"
             "f:        137.125\n"
             "d:        -2.8625\n"
             "h8:       0xFF\n"
             "h16:      0x8765\n"
             "h32:      0xFEDCBA98\n"
             "h64:      0xFEDCBA9876543210\n"
             "...\n");
   yin >> map;

   EXPECT_FALSE(yin.getError());
   EXPECT_TRUE(map.str.equals("hello there"));
   EXPECT_TRUE(map.stdstr == "hello where?");
   EXPECT_EQ(map.u64, 5000000000ULL);
   EXPECT_EQ(map.u32, 4000000000U);
   EXPECT_EQ(map.u16, 65000);
   EXPECT_EQ(map.u8,  255);
   EXPECT_EQ(map.b,   false);
   EXPECT_EQ(map.s64, -5000000000LL);
   EXPECT_EQ(map.s32, -2000000000L);
   EXPECT_EQ(map.s16, -32000);
   EXPECT_EQ(map.s8,  -127);
   EXPECT_EQ(map.f,   137.125);
   EXPECT_EQ(map.d,   -2.8625);
   EXPECT_EQ(map.h8,  Hex8(255));
   EXPECT_EQ(map.h16, Hex16(0x8765));
   EXPECT_EQ(map.h32, Hex32(0xFEDCBA98));
   EXPECT_EQ(map.h64, Hex64(0xFEDCBA9876543210LL));
}


//
// test writing then reading back all built-in scalar types
//
TEST(YamlIOTest, testReadWriteBuiltInTypes)
{
   std::string intermediate;
   {
      BuiltInTypes map;
      map.str = "one two";
      map.stdstr = "three four";
      map.u64 = 6000000000ULL;
      map.u32 = 3000000000U;
      map.u16 = 50000;
      map.u8  = 254;
      map.b   = true;
      map.s64 = -6000000000LL;
      map.s32 = -2000000000;
      map.s16 = -32000;
      map.s8  = -128;
      map.f   = 3.25;
      map.d   = -2.8625;
      map.h8  = 254;
      map.h16 = 50000;
      map.h32 = 3000000000U;
      map.h64 = 6000000000LL;

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << map;
   }

   {
      Input yin(intermediate);
      BuiltInTypes map;
      yin >> map;

      EXPECT_FALSE(yin.getError());
      EXPECT_TRUE(map.str.equals("one two"));
      EXPECT_TRUE(map.stdstr == "three four");
      EXPECT_EQ(map.u64,      6000000000ULL);
      EXPECT_EQ(map.u32,      3000000000U);
      EXPECT_EQ(map.u16,      50000);
      EXPECT_EQ(map.u8,       254);
      EXPECT_EQ(map.b,        true);
      EXPECT_EQ(map.s64,      -6000000000LL);
      EXPECT_EQ(map.s32,      -2000000000L);
      EXPECT_EQ(map.s16,      -32000);
      EXPECT_EQ(map.s8,       -128);
      EXPECT_EQ(map.f,        3.25);
      EXPECT_EQ(map.d,        -2.8625);
      EXPECT_EQ(map.h8,       Hex8(254));
      EXPECT_EQ(map.h16,      Hex16(50000));
      EXPECT_EQ(map.h32,      Hex32(3000000000U));
      EXPECT_EQ(map.h64,      Hex64(6000000000LL));
   }
}

//===----------------------------------------------------------------------===//
//  test endian-aware types
//===----------------------------------------------------------------------===//

struct EndianTypes
{
   typedef polar::utils::internal::PackedEndianSpecificIntegral<
   float, Endianness::Little, polar::utils::UNALIGNED>
   ulittle_float;
   typedef polar::utils::internal::PackedEndianSpecificIntegral<
   double, Endianness::Little, polar::utils::UNALIGNED>
   ulittle_double;

   polar::utils::ulittle64_t u64;
   polar::utils::ulittle32_t u32;
   polar::utils::ulittle16_t u16;
   polar::utils::little64_t s64;
   polar::utils::little32_t s32;
   polar::utils::little16_t s16;
   ulittle_float f;
   ulittle_double d;
};

namespace polar {
namespace yaml {
template <> struct MappingTraits<EndianTypes>
{
   static void mapping(IO &io, EndianTypes &et) {
      io.mapRequired("u64", et.u64);
      io.mapRequired("u32", et.u32);
      io.mapRequired("u16", et.u16);
      io.mapRequired("s64", et.s64);
      io.mapRequired("s32", et.s32);
      io.mapRequired("s16", et.s16);
      io.mapRequired("f", et.f);
      io.mapRequired("d", et.d);
   }
};
}
}

//
// test the reading of all endian scalar conversions
//
TEST(YamlIOTest, testReadEndianTypes)
{
   EndianTypes map;
   Input yin("---\n"
             "u64:      5000000000\n"
             "u32:      4000000000\n"
             "u16:      65000\n"
             "s64:      -5000000000\n"
             "s32:      -2000000000\n"
             "s16:      -32000\n"
             "f:        3.25\n"
             "d:        -2.8625\n"
             "...\n");
   yin >> map;

   EXPECT_FALSE(yin.getError());
   EXPECT_EQ(map.u64, 5000000000ULL);
   EXPECT_EQ(map.u32, 4000000000U);
   EXPECT_EQ(map.u16, 65000);
   EXPECT_EQ(map.s64, -5000000000LL);
   EXPECT_EQ(map.s32, -2000000000L);
   EXPECT_EQ(map.s16, -32000);
   EXPECT_EQ(map.f, 3.25f);
   EXPECT_EQ(map.d, -2.8625);
}

//
// test writing then reading back all endian-aware scalar types
//
TEST(YamlIOTest, testReadWriteEndianTypes)
{
   std::string intermediate;
   {
      EndianTypes map;
      map.u64 = 6000000000ULL;
      map.u32 = 3000000000U;
      map.u16 = 50000;
      map.s64 = -6000000000LL;
      map.s32 = -2000000000;
      map.s16 = -32000;
      map.f = 3.25f;
      map.d = -2.8625;

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << map;
   }

   {
      Input yin(intermediate);
      EndianTypes map;
      yin >> map;

      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(map.u64, 6000000000ULL);
      EXPECT_EQ(map.u32, 3000000000U);
      EXPECT_EQ(map.u16, 50000);
      EXPECT_EQ(map.s64, -6000000000LL);
      EXPECT_EQ(map.s32, -2000000000L);
      EXPECT_EQ(map.s16, -32000);
      EXPECT_EQ(map.f, 3.25f);
      EXPECT_EQ(map.d, -2.8625);
   }
}

struct StringTypes {
   StringRef str1;
   StringRef str2;
   StringRef str3;
   StringRef str4;
   StringRef str5;
   StringRef str6;
   StringRef str7;
   StringRef str8;
   StringRef str9;
   StringRef str10;
   StringRef str11;
   std::string stdstr1;
   std::string stdstr2;
   std::string stdstr3;
   std::string stdstr4;
   std::string stdstr5;
   std::string stdstr6;
   std::string stdstr7;
   std::string stdstr8;
   std::string stdstr9;
   std::string stdstr10;
   std::string stdstr11;
   std::string stdstr12;
};

namespace polar {
namespace yaml {
template <>
struct MappingTraits<StringTypes> {
   static void mapping(IO &io, StringTypes& st) {
      io.mapRequired("str1",      st.str1);
      io.mapRequired("str2",      st.str2);
      io.mapRequired("str3",      st.str3);
      io.mapRequired("str4",      st.str4);
      io.mapRequired("str5",      st.str5);
      io.mapRequired("str6",      st.str6);
      io.mapRequired("str7",      st.str7);
      io.mapRequired("str8",      st.str8);
      io.mapRequired("str9",      st.str9);
      io.mapRequired("str10",     st.str10);
      io.mapRequired("str11",     st.str11);
      io.mapRequired("stdstr1",   st.stdstr1);
      io.mapRequired("stdstr2",   st.stdstr2);
      io.mapRequired("stdstr3",   st.stdstr3);
      io.mapRequired("stdstr4",   st.stdstr4);
      io.mapRequired("stdstr5",   st.stdstr5);
      io.mapRequired("stdstr6",   st.stdstr6);
      io.mapRequired("stdstr7",   st.stdstr7);
      io.mapRequired("stdstr8",   st.stdstr8);
      io.mapRequired("stdstr9",   st.stdstr9);
      io.mapRequired("stdstr10",  st.stdstr10);
      io.mapRequired("stdstr11",  st.stdstr11);
      io.mapRequired("stdstr12",  st.stdstr12);
   }
};
}
}

TEST(YamlIOTest, testReadWriteStringTypes)
{
   std::string intermediate;
   {
      StringTypes map;
      map.str1 = "'aaa";
      map.str2 = "\"bbb";
      map.str3 = "`ccc";
      map.str4 = "@ddd";
      map.str5 = "";
      map.str6 = "0000000004000000";
      map.str7 = "true";
      map.str8 = "FALSE";
      map.str9 = "~";
      map.str10 = "0.2e20";
      map.str11 = "0x30";
      map.stdstr1 = "'eee";
      map.stdstr2 = "\"fff";
      map.stdstr3 = "`ggg";
      map.stdstr4 = "@hhh";
      map.stdstr5 = "";
      map.stdstr6 = "0000000004000000";
      map.stdstr7 = "true";
      map.stdstr8 = "FALSE";
      map.stdstr9 = "~";
      map.stdstr10 = "0.2e20";
      map.stdstr11 = "0x30";
      map.stdstr12 = "- match";

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << map;
   }

   StringRef flowOut(intermediate);
   EXPECT_NE(StringRef::npos, flowOut.find("'''aaa"));
   EXPECT_NE(StringRef::npos, flowOut.find("'\"bbb'"));
   EXPECT_NE(StringRef::npos, flowOut.find("'`ccc'"));
   EXPECT_NE(StringRef::npos, flowOut.find("'@ddd'"));
   EXPECT_NE(StringRef::npos, flowOut.find("''\n"));
   EXPECT_NE(StringRef::npos, flowOut.find("'0000000004000000'\n"));
   EXPECT_NE(StringRef::npos, flowOut.find("'true'\n"));
   EXPECT_NE(StringRef::npos, flowOut.find("'FALSE'\n"));
   EXPECT_NE(StringRef::npos, flowOut.find("'~'\n"));
   EXPECT_NE(StringRef::npos, flowOut.find("'0.2e20'\n"));
   EXPECT_NE(StringRef::npos, flowOut.find("'0x30'\n"));
   EXPECT_NE(StringRef::npos, flowOut.find("'- match'\n"));
   EXPECT_NE(std::string::npos, flowOut.find("'''eee"));
   EXPECT_NE(std::string::npos, flowOut.find("'\"fff'"));
   EXPECT_NE(std::string::npos, flowOut.find("'`ggg'"));
   EXPECT_NE(std::string::npos, flowOut.find("'@hhh'"));
   EXPECT_NE(std::string::npos, flowOut.find("''\n"));
   EXPECT_NE(std::string::npos, flowOut.find("'0000000004000000'\n"));

   {
      Input yin(intermediate);
      StringTypes map;
      yin >> map;

      EXPECT_FALSE(yin.getError());
      EXPECT_TRUE(map.str1.equals("'aaa"));
      EXPECT_TRUE(map.str2.equals("\"bbb"));
      EXPECT_TRUE(map.str3.equals("`ccc"));
      EXPECT_TRUE(map.str4.equals("@ddd"));
      EXPECT_TRUE(map.str5.equals(""));
      EXPECT_TRUE(map.str6.equals("0000000004000000"));
      EXPECT_TRUE(map.stdstr1 == "'eee");
      EXPECT_TRUE(map.stdstr2 == "\"fff");
      EXPECT_TRUE(map.stdstr3 == "`ggg");
      EXPECT_TRUE(map.stdstr4 == "@hhh");
      EXPECT_TRUE(map.stdstr5 == "");
      EXPECT_TRUE(map.stdstr6 == "0000000004000000");
   }
}

//===----------------------------------------------------------------------===//
//  test ScalarEnumerationTraits
//===----------------------------------------------------------------------===//

enum Colors {
   cRed,
   cBlue,
   cGreen,
   cYellow
};

struct ColorMap {
   Colors      c1;
   Colors      c2;
   Colors      c3;
   Colors      c4;
   Colors      c5;
   Colors      c6;
};

namespace polar {
namespace yaml {
template <>
struct ScalarEnumerationTraits<Colors> {
   static void enumeration(IO &io, Colors &value) {
      io.enumCase(value, "red",   cRed);
      io.enumCase(value, "blue",  cBlue);
      io.enumCase(value, "green", cGreen);
      io.enumCase(value, "yellow",cYellow);
   }
};
template <>
struct MappingTraits<ColorMap> {
   static void mapping(IO &io, ColorMap& c) {
      io.mapRequired("c1", c.c1);
      io.mapRequired("c2", c.c2);
      io.mapRequired("c3", c.c3);
      io.mapOptional("c4", c.c4, cBlue);   // supplies default
      io.mapOptional("c5", c.c5, cYellow); // supplies default
      io.mapOptional("c6", c.c6, cRed);    // supplies default
   }
};
}
}


//
// test reading enumerated scalars
//
TEST(YamlIOTest, testEnumRead) {
   ColorMap map;
   Input yin("---\n"
             "c1:  blue\n"
             "c2:  red\n"
             "c3:  green\n"
             "c5:  yellow\n"
             "...\n");
   yin >> map;

   EXPECT_FALSE(yin.getError());
   EXPECT_EQ(cBlue,  map.c1);
   EXPECT_EQ(cRed,   map.c2);
   EXPECT_EQ(cGreen, map.c3);
   EXPECT_EQ(cBlue,  map.c4);  // tests default
   EXPECT_EQ(cYellow,map.c5);  // tests overridden
   EXPECT_EQ(cRed,   map.c6);  // tests default
}



//===----------------------------------------------------------------------===//
//  test ScalarBitSetTraits
//===----------------------------------------------------------------------===//

enum MyFlags {
   flagNone    = 0,
   flagBig     = 1 << 0,
   flagFlat    = 1 << 1,
   flagRound   = 1 << 2,
   flagPointy  = 1 << 3
};
inline MyFlags operator|(MyFlags a, MyFlags b) {
   return static_cast<MyFlags>(
            static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

struct FlagsMap {
   MyFlags     f1;
   MyFlags     f2;
   MyFlags     f3;
   MyFlags     f4;
};


namespace polar {
namespace yaml {
template <>
struct ScalarBitSetTraits<MyFlags> {
   static void bitset(IO &io, MyFlags &value) {
      io.bitSetCase(value, "big",   flagBig);
      io.bitSetCase(value, "flat",  flagFlat);
      io.bitSetCase(value, "round", flagRound);
      io.bitSetCase(value, "pointy",flagPointy);
   }
};
template <>
struct MappingTraits<FlagsMap> {
   static void mapping(IO &io, FlagsMap& c) {
      io.mapRequired("f1", c.f1);
      io.mapRequired("f2", c.f2);
      io.mapRequired("f3", c.f3);
      io.mapOptional("f4", c.f4, MyFlags(flagRound));
   }
};
}
}


//
// test reading flow sequence representing bit-mask values
//
TEST(YamlIOTest, testFlagsRead) {
   FlagsMap map;
   Input yin("---\n"
             "f1:  [ big ]\n"
             "f2:  [ round, flat ]\n"
             "f3:  []\n"
             "...\n");
   yin >> map;

   EXPECT_FALSE(yin.getError());
   EXPECT_EQ(flagBig,              map.f1);
   EXPECT_EQ(flagRound|flagFlat,   map.f2);
   EXPECT_EQ(flagNone,             map.f3);  // check empty set
   EXPECT_EQ(flagRound,            map.f4);  // check optional key
}


//
// test writing then reading back bit-mask values
//
TEST(YamlIOTest, testReadWriteFlags)
{
   std::string intermediate;
   {
      FlagsMap map;
      map.f1 = flagBig;
      map.f2 = flagRound | flagFlat;
      map.f3 = flagNone;
      map.f4 = flagNone;

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << map;
   }

   {
      Input yin(intermediate);
      FlagsMap map2;
      yin >> map2;

      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(flagBig,              map2.f1);
      EXPECT_EQ(flagRound|flagFlat,   map2.f2);
      EXPECT_EQ(flagNone,             map2.f3);
      //EXPECT_EQ(flagRound,            map2.f4);  // check optional key
   }
}



//===----------------------------------------------------------------------===//
//  test ScalarTraits
//===----------------------------------------------------------------------===//

struct MyCustomType {
   int length;
   int width;
};

struct MyCustomTypeMap {
   MyCustomType     f1;
   MyCustomType     f2;
   int              f3;
};


namespace polar {
namespace yaml {
template <>
struct MappingTraits<MyCustomTypeMap> {
   static void mapping(IO &io, MyCustomTypeMap& s) {
      io.mapRequired("f1", s.f1);
      io.mapRequired("f2", s.f2);
      io.mapRequired("f3", s.f3);
   }
};
// MyCustomType is formatted as a yaml scalar.  A value of
// {length=3, width=4} would be represented in yaml as "3 by 4".
template<>
struct ScalarTraits<MyCustomType> {
   static void output(const MyCustomType &value, void* ctxt, RawOutStream &out) {
      out << polar::utils::format("%d by %d", value.length, value.width);
   }
   static StringRef input(StringRef scalar, void* ctxt, MyCustomType &value) {
      size_t byStart = scalar.find("by");
      if ( byStart != StringRef::npos ) {
         StringRef lenStr = scalar.slice(0, byStart);
         lenStr = lenStr.rtrim();
         if ( lenStr.getAsInteger(0, value.length) ) {
            return "malformed length";
         }
         StringRef widthStr = scalar.dropFront(byStart+2);
         widthStr = widthStr.ltrim();
         if ( widthStr.getAsInteger(0, value.width) ) {
            return "malformed width";
         }
         return StringRef();
      }
      else {
         return "malformed by";
      }
   }
   static QuotingType mustQuote(StringRef) { return QuotingType::Single; }
};
}
}


//
// test writing then reading back custom values
//
TEST(YamlIOTest, testReadWriteMyCustomType)
{
   std::string intermediate;
   {
      MyCustomTypeMap map;
      map.f1.length = 1;
      map.f1.width  = 4;
      map.f2.length = 100;
      map.f2.width  = 400;
      map.f3 = 10;

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << map;
   }

   {
      Input yin(intermediate);
      MyCustomTypeMap map2;
      yin >> map2;

      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(1,      map2.f1.length);
      EXPECT_EQ(4,      map2.f1.width);
      EXPECT_EQ(100,    map2.f2.length);
      EXPECT_EQ(400,    map2.f2.width);
      EXPECT_EQ(10,     map2.f3);
   }
}


//===----------------------------------------------------------------------===//
//  test BlockScalarTraits
//===----------------------------------------------------------------------===//

struct MultilineStringType {
   std::string str;
};

struct MultilineStringTypeMap {
   MultilineStringType name;
   MultilineStringType description;
   MultilineStringType ingredients;
   MultilineStringType recipes;
   MultilineStringType warningLabels;
   MultilineStringType documentation;
   int price;
};

namespace polar {
namespace yaml {
template <>
struct MappingTraits<MultilineStringTypeMap> {
   static void mapping(IO &io, MultilineStringTypeMap& s) {
      io.mapRequired("name", s.name);
      io.mapRequired("description", s.description);
      io.mapRequired("ingredients", s.ingredients);
      io.mapRequired("recipes", s.recipes);
      io.mapRequired("warningLabels", s.warningLabels);
      io.mapRequired("documentation", s.documentation);
      io.mapRequired("price", s.price);
   }
};

// MultilineStringType is formatted as a yaml block literal scalar. A value of
// "Hello\nWorld" would be represented in yaml as
//  |
//    Hello
//    World
template <>
struct BlockScalarTraits<MultilineStringType> {
   static void output(const MultilineStringType &value, void *ctxt,
                      RawOutStream &out) {
      out << value.str;
   }
   static StringRef input(StringRef scalar, void *ctxt,
                          MultilineStringType &value) {
      value.str = scalar.getStr();
      return StringRef();
   }
};
}
}

POLAR_YAML_IS_DOCUMENT_LIST_VECTOR(MultilineStringType)

//
// test writing then reading back custom values
//
TEST(YamlIOTest, testReadWriteMultilineStringType)
{
   std::string intermediate;
   {
      MultilineStringTypeMap map;
      map.name.str = "An Item";
      map.description.str = "Hello\nWorld";
      map.ingredients.str = "SubItem 1\nSub Item 2\n\nSub Item 3\n";
      map.recipes.str = "\n\nTest 1\n\n\n";
      map.warningLabels.str = "";
      map.documentation.str = "\n\n";
      map.price = 350;

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << map;
   }
   {
      Input yin(intermediate);
      MultilineStringTypeMap map2;
      yin >> map2;

      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(map2.name.str, "An Item\n");
      EXPECT_EQ(map2.description.str, "Hello\nWorld\n");
      EXPECT_EQ(map2.ingredients.str, "SubItem 1\nSub Item 2\n\nSub Item 3\n");
      EXPECT_EQ(map2.recipes.str, "\n\nTest 1\n");
      EXPECT_TRUE(map2.warningLabels.str.empty());
      EXPECT_TRUE(map2.documentation.str.empty());
      EXPECT_EQ(map2.price, 350);
   }
}

//
// test writing then reading back custom values
//
TEST(YamlIOTest, testReadWriteBlockScalarDocuments)
{
   std::string intermediate;
   {
      std::vector<MultilineStringType> documents;
      MultilineStringType doc;
      doc.str = "Hello\nWorld";
      documents.push_back(doc);

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << documents;

      // Verify that the block scalar header was written out on the same line
      // as the document marker.
      EXPECT_NE(StringRef::npos, StringRef(ostr.getStr()).find("--- |"));
   }
   {
      Input yin(intermediate);
      std::vector<MultilineStringType> documents2;
      yin >> documents2;

      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(documents2.size(), size_t(1));
      EXPECT_EQ(documents2[0].str, "Hello\nWorld\n");
   }
}

TEST(YamlIOTest, testReadWriteBlockScalarValue)
{
   std::string intermediate;
   {
      MultilineStringType doc;
      doc.str = "Just a block\nscalar doc";

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << doc;
   }
   {
      Input yin(intermediate);
      MultilineStringType doc;
      yin >> doc;

      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(doc.str, "Just a block\nscalar doc\n");
   }
}

//===----------------------------------------------------------------------===//
//  test flow sequences
//===----------------------------------------------------------------------===//

POLAR_YAML_STRONG_TYPEDEF(int, MyNumber)
POLAR_YAML_IS_FLOW_SEQUENCE_VECTOR(MyNumber)
POLAR_YAML_STRONG_TYPEDEF(StringRef, MyString)
POLAR_YAML_IS_FLOW_SEQUENCE_VECTOR(MyString)

namespace polar {
   namespace yaml {
   template<>
   struct ScalarTraits<MyNumber> {
      static void output(const MyNumber &value, void *, RawOutStream &out) {
         out << value;
      }

      static StringRef input(StringRef scalar, void *, MyNumber &value) {
         long long n;
         if (get_as_signed_integer(scalar, 0, n) )
            return "invalid number";
         value = n;
         return StringRef();
      }

      static QuotingType mustQuote(StringRef) { return QuotingType::None; }
   };

   template <> struct ScalarTraits<MyString> {
      using Impl = ScalarTraits<StringRef>;
      static void output(const MyString &V, void *Ctx, RawOutStream &OS) {
         Impl::output(V, Ctx, OS);
      }
      static StringRef input(StringRef S, void *Ctx, MyString &V) {
         return Impl::input(S, Ctx, V.value);
      }
      static QuotingType mustQuote(StringRef S) {
         return Impl::mustQuote(S);
      }
   };
   }
}

struct NameAndNumbers {
   StringRef               name;
   std::vector<MyString>         strings;
   std::vector<MyNumber>         single;
   std::vector<MyNumber>         numbers;
};

namespace polar {
namespace yaml {
template <>
struct MappingTraits<NameAndNumbers>
{
   static void mapping(IO &io, NameAndNumbers& nn) {
      io.mapRequired("name",     nn.name);
      io.mapRequired("strings",  nn.strings);
      io.mapRequired("single",   nn.single);
      io.mapRequired("numbers",  nn.numbers);
   }
};
}
}

typedef std::vector<MyNumber> MyNumberFlowSequence;

POLAR_YAML_IS_SEQUENCE_VECTOR(MyNumberFlowSequence)

struct NameAndNumbersFlow {
   StringRef                    name;
   std::vector<MyNumberFlowSequence>  sequenceOfNumbers;
};

namespace polar {
namespace yaml {
template <>
struct MappingTraits<NameAndNumbersFlow> {
   static void mapping(IO &io, NameAndNumbersFlow& nn) {
      io.mapRequired("name",     nn.name);
      io.mapRequired("sequenceOfNumbers",  nn.sequenceOfNumbers);
   }
};
}
}

//
// test writing then reading back custom values
//
TEST(YamlIOTest, testReadWriteMyFlowSequence)
{
   std::string intermediate;
   {
      NameAndNumbers map;
      map.name  = "hello";
      map.strings.push_back(StringRef("one"));
      map.strings.push_back(StringRef("two"));
      map.single.push_back(1);
      map.numbers.push_back(10);
      map.numbers.push_back(-30);
      map.numbers.push_back(1024);

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << map;

      // Verify sequences were written in flow style
      ostr.flush();
      StringRef flowOut(intermediate);
      EXPECT_NE(StringRef::npos, flowOut.find("one, two"));
      EXPECT_NE(StringRef::npos, flowOut.find("10, -30, 1024"));
   }

   {
      Input yin(intermediate);
      NameAndNumbers map2;
      yin >> map2;

      EXPECT_FALSE(yin.getError());
      EXPECT_TRUE(map2.name.equals("hello"));
      EXPECT_EQ(map2.strings.size(), 2UL);
      EXPECT_TRUE(map2.strings[0].value.equals("one"));
      EXPECT_TRUE(map2.strings[1].value.equals("two"));
      EXPECT_EQ(map2.single.size(), 1UL);
      EXPECT_EQ(1,       map2.single[0]);
      EXPECT_EQ(map2.numbers.size(), 3UL);
      EXPECT_EQ(10,      map2.numbers[0]);
      EXPECT_EQ(-30,     map2.numbers[1]);
      EXPECT_EQ(1024,    map2.numbers[2]);
   }
}

//
// test writing then reading back a sequence of flow sequences.
//
TEST(YamlIOTest, testReadWriteSequenceOfMyFlowSequence)
{
   std::string intermediate;
   {
      NameAndNumbersFlow map;
      map.name  = "hello";
      MyNumberFlowSequence single = { 0 };
      MyNumberFlowSequence numbers = { 12, 1, -512 };
      map.sequenceOfNumbers.push_back(single);
      map.sequenceOfNumbers.push_back(numbers);
      map.sequenceOfNumbers.push_back(MyNumberFlowSequence());

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << map;

      // Verify sequences were written in flow style
      // and that the parent sequence used '-'.
      ostr.flush();
      StringRef flowOut(intermediate);
      EXPECT_NE(StringRef::npos, flowOut.find("- [ 0 ]"));
      EXPECT_NE(StringRef::npos, flowOut.find("- [ 12, 1, -512 ]"));
      EXPECT_NE(StringRef::npos, flowOut.find("- [  ]"));
   }

   {
      Input yin(intermediate);
      NameAndNumbersFlow map2;
      yin >> map2;

      EXPECT_FALSE(yin.getError());
      EXPECT_TRUE(map2.name.equals("hello"));
      EXPECT_EQ(map2.sequenceOfNumbers.size(), 3UL);
      EXPECT_EQ(map2.sequenceOfNumbers[0].size(), 1UL);
      EXPECT_EQ(0,    map2.sequenceOfNumbers[0][0]);
      EXPECT_EQ(map2.sequenceOfNumbers[1].size(), 3UL);
      EXPECT_EQ(12,   map2.sequenceOfNumbers[1][0]);
      EXPECT_EQ(1,    map2.sequenceOfNumbers[1][1]);
      EXPECT_EQ(-512, map2.sequenceOfNumbers[1][2]);
      EXPECT_TRUE(map2.sequenceOfNumbers[2].empty());
   }
}

//===----------------------------------------------------------------------===//
//  test normalizing/denormalizing
//===----------------------------------------------------------------------===//

POLAR_YAML_STRONG_TYPEDEF(uint32_t, TotalSeconds)

typedef std::vector<TotalSeconds> SecondsSequence;

POLAR_YAML_IS_SEQUENCE_VECTOR(TotalSeconds)


namespace polar {
   namespace yaml {
   template <>
   struct MappingTraits<TotalSeconds> {

      class NormalizedSeconds {
      public:
         NormalizedSeconds(IO &io)
            : hours(0), minutes(0), seconds(0) {
         }
         NormalizedSeconds(IO &, TotalSeconds &secs)
            : hours(secs/3600),
              minutes((secs - (hours*3600))/60),
              seconds(secs % 60) {
         }
         TotalSeconds denormalize(IO &) {
            return TotalSeconds(hours*3600 + minutes*60 + seconds);
         }

         uint32_t     hours;
         uint8_t      minutes;
         uint8_t      seconds;
      };

      static void mapping(IO &io, TotalSeconds &secs) {
         MappingNormalization<NormalizedSeconds, TotalSeconds> keys(io, secs);

         io.mapOptional("hours",    keys->hours,    (uint32_t)0);
         io.mapOptional("minutes",  keys->minutes,  (uint8_t)0);
         io.mapRequired("seconds",  keys->seconds);
      }
   };
   }
}


//
// test the reading of a yaml sequence of mappings
//
TEST(YamlIOTest, testReadMySecondsSequence)
{
   SecondsSequence seq;
   Input yin("---\n - hours:  1\n   seconds:  5\n - seconds:  59\n...\n");
   yin >> seq;

   EXPECT_FALSE(yin.getError());
   EXPECT_EQ(seq.size(), 2UL);
   EXPECT_EQ(seq[0], 3605U);
   EXPECT_EQ(seq[1], 59U);
}


//
// test writing then reading back custom values
//
TEST(YamlIOTest, testReadWriteMySecondsSequence)
{
   std::string intermediate;
   {
      SecondsSequence seq;
      seq.push_back(4000);
      seq.push_back(500);
      seq.push_back(59);

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << seq;
   }
   {
      Input yin(intermediate);
      SecondsSequence seq2;
      yin >> seq2;

      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(seq2.size(), 3UL);
      EXPECT_EQ(seq2[0], 4000U);
      EXPECT_EQ(seq2[1], 500U);
      EXPECT_EQ(seq2[2], 59U);
   }
}


//===----------------------------------------------------------------------===//
//  test dynamic typing
//===----------------------------------------------------------------------===//

enum AFlags {
   a1,
   a2,
   a3
};

enum BFlags {
   b1,
   b2,
   b3
};

enum Kind {
   kindA,
   kindB
};

struct KindAndFlags {
   KindAndFlags() : kind(kindA), flags(0) { }
   KindAndFlags(Kind k, uint32_t f) : kind(k), flags(f) { }
   Kind        kind;
   uint32_t    flags;
};

typedef std::vector<KindAndFlags> KindAndFlagsSequence;

POLAR_YAML_IS_SEQUENCE_VECTOR(KindAndFlags)

namespace polar {
   namespace yaml {
   template <>
   struct ScalarEnumerationTraits<AFlags> {
      static void enumeration(IO &io, AFlags &value) {
         io.enumCase(value, "a1",  a1);
         io.enumCase(value, "a2",  a2);
         io.enumCase(value, "a3",  a3);
      }
   };
   template <>
   struct ScalarEnumerationTraits<BFlags> {
      static void enumeration(IO &io, BFlags &value) {
         io.enumCase(value, "b1",  b1);
         io.enumCase(value, "b2",  b2);
         io.enumCase(value, "b3",  b3);
      }
   };
   template <>
   struct ScalarEnumerationTraits<Kind> {
      static void enumeration(IO &io, Kind &value) {
         io.enumCase(value, "A",  kindA);
         io.enumCase(value, "B",  kindB);
      }
   };
   template <>
   struct MappingTraits<KindAndFlags> {
      static void mapping(IO &io, KindAndFlags& kf) {
         io.mapRequired("kind",  kf.kind);
         // Type of "flags" field varies depending on "kind" field.
         // Use memcpy here to avoid breaking strict aliasing rules.
         if (kf.kind == kindA) {
            AFlags aflags = static_cast<AFlags>(kf.flags);
            io.mapRequired("flags", aflags);
            kf.flags = aflags;
         } else {
            BFlags bflags = static_cast<BFlags>(kf.flags);
            io.mapRequired("flags", bflags);
            kf.flags = bflags;
         }
      }
   };
   }
}


//
// test the reading of a yaml sequence dynamic types
//
TEST(YamlIOTest, testReadKindAndFlagsSequence)
{
   KindAndFlagsSequence seq;
   Input yin("---\n - kind:  A\n   flags:  a2\n - kind:  B\n   flags:  b1\n...\n");
   yin >> seq;

   EXPECT_FALSE(yin.getError());
   EXPECT_EQ(seq.size(), 2UL);
   EXPECT_EQ(seq[0].kind,  kindA);
   EXPECT_EQ(seq[0].flags, (uint32_t)a2);
   EXPECT_EQ(seq[1].kind,  kindB);
   EXPECT_EQ(seq[1].flags, (uint32_t)b1);
}

//
// test writing then reading back dynamic types
//
TEST(YamlIOTest, testReadWriteKindAndFlagsSequence)
{
   std::string intermediate;
   {
      KindAndFlagsSequence seq;
      seq.push_back(KindAndFlags(kindA,a1));
      seq.push_back(KindAndFlags(kindB,b1));
      seq.push_back(KindAndFlags(kindA,a2));
      seq.push_back(KindAndFlags(kindB,b2));
      seq.push_back(KindAndFlags(kindA,a3));

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << seq;
   }
   {
      Input yin(intermediate);
      KindAndFlagsSequence seq2;
      yin >> seq2;

      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(seq2.size(), 5UL);
      EXPECT_EQ(seq2[0].kind,  kindA);
      EXPECT_EQ(seq2[0].flags, (uint32_t)a1);
      EXPECT_EQ(seq2[1].kind,  kindB);
      EXPECT_EQ(seq2[1].flags, (uint32_t)b1);
      EXPECT_EQ(seq2[2].kind,  kindA);
      EXPECT_EQ(seq2[2].flags, (uint32_t)a2);
      EXPECT_EQ(seq2[3].kind,  kindB);
      EXPECT_EQ(seq2[3].flags, (uint32_t)b2);
      EXPECT_EQ(seq2[4].kind,  kindA);
      EXPECT_EQ(seq2[4].flags, (uint32_t)a3);
   }
}

//===----------------------------------------------------------------------===//
//  test document list
//===----------------------------------------------------------------------===//

struct FooBarMap {
   int foo;
   int bar;
};
typedef std::vector<FooBarMap> FooBarMapDocumentList;

POLAR_YAML_IS_DOCUMENT_LIST_VECTOR(FooBarMap)


namespace polar {
   namespace yaml {
   template <>
   struct MappingTraits<FooBarMap> {
      static void mapping(IO &io, FooBarMap& fb) {
         io.mapRequired("foo",    fb.foo);
         io.mapRequired("bar",    fb.bar);
      }
   };
   }
}


//
// test the reading of a yaml mapping
//
TEST(YamlIOTest, testDocRead)
{
   FooBarMap doc;
   Input yin("---\nfoo:  3\nbar:  5\n...\n");
   yin >> doc;

   EXPECT_FALSE(yin.getError());
   EXPECT_EQ(doc.foo, 3);
   EXPECT_EQ(doc.bar,5);
}



//
// test writing then reading back a sequence of mappings
//
TEST(YamlIOTest, testSequenceDocListWriteAndRead) {
   std::string intermediate;
   {
      FooBarMap doc1;
      doc1.foo = 10;
      doc1.bar = -3;
      FooBarMap doc2;
      doc2.foo = 257;
      doc2.bar = 0;
      std::vector<FooBarMap> docList;
      docList.push_back(doc1);
      docList.push_back(doc2);

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << docList;
   }


   {
      Input yin(intermediate);
      std::vector<FooBarMap> docList2;
      yin >> docList2;

      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(docList2.size(), 2UL);
      FooBarMap& map1 = docList2[0];
      FooBarMap& map2 = docList2[1];
      EXPECT_EQ(map1.foo, 10);
      EXPECT_EQ(map1.bar, -3);
      EXPECT_EQ(map2.foo, 257);
      EXPECT_EQ(map2.bar, 0);
   }
}

//===----------------------------------------------------------------------===//
//  test document tags
//===----------------------------------------------------------------------===//

struct MyDouble {
   MyDouble() : value(0.0) { }
   MyDouble(double x) : value(x) { }
   double value;
};

POLAR_YAML_IS_DOCUMENT_LIST_VECTOR(MyDouble)


namespace polar {
   namespace yaml {
   template <>
   struct MappingTraits<MyDouble> {
      static void mapping(IO &io, MyDouble &d) {
         if (io.mapTag("!decimal", true)) {
            mappingDecimal(io, d);
         } else if (io.mapTag("!fraction")) {
            mappingFraction(io, d);
         }
      }
      static void mappingDecimal(IO &io, MyDouble &d) {
         io.mapRequired("value", d.value);
      }
      static void mappingFraction(IO &io, MyDouble &d) {
         double num, denom;
         io.mapRequired("numerator",      num);
         io.mapRequired("denominator",    denom);
         // convert fraction to double
         d.value = num/denom;
      }
   };
   }
}


//
// test the reading of two different tagged yaml documents.
//
TEST(YamlIOTest, testTaggedDocuments)
{
   std::vector<MyDouble> docList;
   Input yin("--- !decimal\nvalue:  3.0\n"
             "--- !fraction\nnumerator:  9.0\ndenominator:  2\n...\n");
   yin >> docList;
   EXPECT_FALSE(yin.getError());
   EXPECT_EQ(docList.size(), 2UL);
   EXPECT_EQ(docList[0].value, 3.0);
   EXPECT_EQ(docList[1].value, 4.5);
}



//
// test writing then reading back tagged documents
//
TEST(YamlIOTest, testTaggedDocumentsWriteAndRead)
{
   std::string intermediate;
   {
      MyDouble a(10.25);
      MyDouble b(-3.75);
      std::vector<MyDouble> docList;
      docList.push_back(a);
      docList.push_back(b);

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << docList;
   }

   {
      Input yin(intermediate);
      std::vector<MyDouble> docList2;
      yin >> docList2;

      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(docList2.size(), 2UL);
      EXPECT_EQ(docList2[0].value, 10.25);
      EXPECT_EQ(docList2[1].value, -3.75);
   }
}


//===----------------------------------------------------------------------===//
//  test mapping validation
//===----------------------------------------------------------------------===//

struct MyValidation {
   double value;
};

POLAR_YAML_IS_DOCUMENT_LIST_VECTOR(MyValidation)

namespace polar {
   namespace yaml {
   template <>
   struct MappingTraits<MyValidation> {
      static void mapping(IO &io, MyValidation &d) {
         io.mapRequired("value", d.value);
      }
      static StringRef validate(IO &io, MyValidation &d) {
         if (d.value < 0)
            return "negative value";
         return StringRef();
      }
   };
   }
}


//
// test that validate() is called and complains about the negative value.
//
TEST(YamlIOTest, testValidatingInput)
{
   std::vector<MyValidation> docList;
   Input yin("--- \nvalue:  3.0\n"
             "--- \nvalue:  -1.0\n...\n",
             nullptr, suppress_error_messages);
   yin >> docList;
   EXPECT_TRUE(!!yin.getError());
}

//===----------------------------------------------------------------------===//
//  test flow mapping
//===----------------------------------------------------------------------===//

struct FlowFooBar {
   int foo;
   int bar;

   FlowFooBar() : foo(0), bar(0) {}
   FlowFooBar(int foo, int bar) : foo(foo), bar(bar) {}
};

typedef std::vector<FlowFooBar> FlowFooBarSequence;

POLAR_YAML_IS_SEQUENCE_VECTOR(FlowFooBar)

struct FlowFooBarDoc {
   FlowFooBar attribute;
   FlowFooBarSequence seq;
};

namespace polar {
namespace yaml {
template <>
struct MappingTraits<FlowFooBar> {
   static void mapping(IO &io, FlowFooBar &fb) {
      io.mapRequired("foo", fb.foo);
      io.mapRequired("bar", fb.bar);
   }

   static const bool sm_flow = true;
};

template <>
struct MappingTraits<FlowFooBarDoc> {
   static void mapping(IO &io, FlowFooBarDoc &fb) {
      io.mapRequired("attribute", fb.attribute);
      io.mapRequired("seq", fb.seq);
   }
};
}
}

//
// test writing then reading back custom mappings
//
TEST(YamlIOTest, testReadWriteMyFlowMapping)
{
   std::string intermediate;
   {
      FlowFooBarDoc doc;
      doc.attribute = FlowFooBar(42, 907);
      doc.seq.push_back(FlowFooBar(1, 2));
      doc.seq.push_back(FlowFooBar(0, 0));
      doc.seq.push_back(FlowFooBar(-1, 1024));

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << doc;

      // Verify that mappings were written in flow style
      ostr.flush();
      StringRef flowOut(intermediate);
      EXPECT_NE(StringRef::npos, flowOut.find("{ foo: 42, bar: 907 }"));
      EXPECT_NE(StringRef::npos, flowOut.find("- { foo: 1, bar: 2 }"));
      EXPECT_NE(StringRef::npos, flowOut.find("- { foo: 0, bar: 0 }"));
      EXPECT_NE(StringRef::npos, flowOut.find("- { foo: -1, bar: 1024 }"));
   }

   {
      Input yin(intermediate);
      FlowFooBarDoc doc2;
      yin >> doc2;

      EXPECT_FALSE(yin.getError());
      EXPECT_EQ(doc2.attribute.foo, 42);
      EXPECT_EQ(doc2.attribute.bar, 907);
      EXPECT_EQ(doc2.seq.size(), 3UL);
      EXPECT_EQ(doc2.seq[0].foo, 1);
      EXPECT_EQ(doc2.seq[0].bar, 2);
      EXPECT_EQ(doc2.seq[1].foo, 0);
      EXPECT_EQ(doc2.seq[1].bar, 0);
      EXPECT_EQ(doc2.seq[2].foo, -1);
      EXPECT_EQ(doc2.seq[2].bar, 1024);
   }
}

//===----------------------------------------------------------------------===//
//  test error handling
//===----------------------------------------------------------------------===//

//
// test error handling of unknown enumerated scalar
//
TEST(YamlIOTest, testColorsReadError)
{
   ColorMap map;
   Input yin("---\n"
             "c1:  blue\n"
             "c2:  purple\n"
             "c3:  green\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> map;
   EXPECT_TRUE(!!yin.getError());
}


//
// test error handling of flow sequence with unknown value
//
TEST(YamlIOTest, testFlagsReadError)
{
   FlagsMap map;
   Input yin("---\n"
             "f1:  [ big ]\n"
             "f2:  [ round, hollow ]\n"
             "f3:  []\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> map;

   EXPECT_TRUE(!!yin.getError());
}


//
// test error handling reading built-in uint8_t type
//
TEST(YamlIOTest, testReadBuiltInTypesUint8Error)
{
   std::vector<uint8_t> seq;
   Input yin("---\n"
             "- 255\n"
             "- 0\n"
             "- 257\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}


//
// test error handling reading built-in uint16_t type
//
TEST(YamlIOTest, testReadBuiltInTypesUint16Error)
{
   std::vector<uint16_t> seq;
   Input yin("---\n"
             "- 65535\n"
             "- 0\n"
             "- 66000\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}


//
// test error handling reading built-in uint32_t type
//
TEST(YamlIOTest, testReadBuiltInTypesUint32Error)
{
   std::vector<uint32_t> seq;
   Input yin("---\n"
             "- 4000000000\n"
             "- 0\n"
             "- 5000000000\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}


//
// test error handling reading built-in uint64_t type
//
TEST(YamlIOTest, testReadBuiltInTypesUint64Error)
{
   std::vector<uint64_t> seq;
   Input yin("---\n"
             "- 18446744073709551615\n"
             "- 0\n"
             "- 19446744073709551615\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}


//
// test error handling reading built-in int8_t type
//
TEST(YamlIOTest, testReadBuiltInTypesint8OverError)
{
   std::vector<int8_t> seq;
   Input yin("---\n"
             "- -128\n"
             "- 0\n"
             "- 127\n"
             "- 128\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}

//
// test error handling reading built-in int8_t type
//
TEST(YamlIOTest, testReadBuiltInTypesint8UnderError)
{
   std::vector<int8_t> seq;
   Input yin("---\n"
             "- -128\n"
             "- 0\n"
             "- 127\n"
             "- -129\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}


//
// test error handling reading built-in int16_t type
//
TEST(YamlIOTest, testReadBuiltInTypesint16UnderError)
{
   std::vector<int16_t> seq;
   Input yin("---\n"
             "- 32767\n"
             "- 0\n"
             "- -32768\n"
             "- -32769\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}


//
// test error handling reading built-in int16_t type
//
TEST(YamlIOTest, testReadBuiltInTypesint16OverError)
{
   std::vector<int16_t> seq;
   Input yin("---\n"
             "- 32767\n"
             "- 0\n"
             "- -32768\n"
             "- 32768\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}


//
// test error handling reading built-in int32_t type
//
TEST(YamlIOTest, testReadBuiltInTypesint32UnderError)
{
   std::vector<int32_t> seq;
   Input yin("---\n"
             "- 2147483647\n"
             "- 0\n"
             "- -2147483648\n"
             "- -2147483649\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}

//
// test error handling reading built-in int32_t type
//
TEST(YamlIOTest, testReadBuiltInTypesint32OverError)
{
   std::vector<int32_t> seq;
   Input yin("---\n"
             "- 2147483647\n"
             "- 0\n"
             "- -2147483648\n"
             "- 2147483649\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}


//
// test error handling reading built-in int64_t type
//
TEST(YamlIOTest, testReadBuiltInTypesint64UnderError)
{
   std::vector<int64_t> seq;
   Input yin("---\n"
             "- -9223372036854775808\n"
             "- 0\n"
             "- 9223372036854775807\n"
             "- -9223372036854775809\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}

//
// test error handling reading built-in int64_t type
//
TEST(YamlIOTest, testReadBuiltInTypesint64OverError)
{
   std::vector<int64_t> seq;
   Input yin("---\n"
             "- -9223372036854775808\n"
             "- 0\n"
             "- 9223372036854775807\n"
             "- 9223372036854775809\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}

//
// test error handling reading built-in float type
//
TEST(YamlIOTest, testReadBuiltInTypesFloatError)
{
   std::vector<float> seq;
   Input yin("---\n"
             "- 0.0\n"
             "- 1000.1\n"
             "- -123.456\n"
             "- 1.2.3\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}

//
// test error handling reading built-in float type
//
TEST(YamlIOTest, testReadBuiltInTypesDoubleError)
{
   std::vector<double> seq;
   Input yin("---\n"
             "- 0.0\n"
             "- 1000.1\n"
             "- -123.456\n"
             "- 1.2.3\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}

//
// test error handling reading built-in Hex8 type
//
POLAR_YAML_IS_SEQUENCE_VECTOR(Hex8)
TEST(YamlIOTest, testReadBuiltInTypesHex8Error)
{
   std::vector<Hex8> seq;
   Input yin("---\n"
             "- 0x12\n"
             "- 0xFE\n"
             "- 0x123\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}


//
// test error handling reading built-in Hex16 type
//
POLAR_YAML_IS_SEQUENCE_VECTOR(Hex16)
TEST(YamlIOTest, testReadBuiltInTypesHex16Error)
{
   std::vector<Hex16> seq;
   Input yin("---\n"
             "- 0x0012\n"
             "- 0xFEFF\n"
             "- 0x12345\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}

//
// test error handling reading built-in Hex32 type
//
POLAR_YAML_IS_SEQUENCE_VECTOR(Hex32)
TEST(YamlIOTest, testReadBuiltInTypesHex32Error) {
   std::vector<Hex32> seq;
   Input yin("---\n"
             "- 0x0012\n"
             "- 0xFEFF0000\n"
             "- 0x1234556789\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}

//
// test error handling reading built-in Hex64 type
//
POLAR_YAML_IS_SEQUENCE_VECTOR(Hex64)
TEST(YamlIOTest, testReadBuiltInTypesHex64Error)
{
   std::vector<Hex64> seq;
   Input yin("---\n"
             "- 0x0012\n"
             "- 0xFFEEDDCCBBAA9988\n"
             "- 0x12345567890ABCDEF0\n"
             "...\n",
             /*Ctxt=*/nullptr,
             suppress_error_messages);
   yin >> seq;

   EXPECT_TRUE(!!yin.getError());
}

TEST(YamlIOTest, testMalformedMapFailsGracefully)
{
   FooBar doc;
   {
      // We pass the suppress_error_messages handler to handle the error
      // message generated in the constructor of Input.
      Input yin("{foo:3, bar: 5}", /*Ctxt=*/nullptr, suppress_error_messages);
      yin >> doc;
      EXPECT_TRUE(!!yin.getError());
   }

   {
      Input yin("---\nfoo:3\nbar: 5\n...\n", /*Ctxt=*/nullptr, suppress_error_messages);
      yin >> doc;
      EXPECT_TRUE(!!yin.getError());
   }
}

struct OptionalTest {
   std::vector<int> numbers;
};

struct OptionalTestSeq {
   std::vector<OptionalTest> tests;
};

POLAR_YAML_IS_SEQUENCE_VECTOR(OptionalTest)
namespace polar {
   namespace yaml {
   template <>
   struct MappingTraits<OptionalTest> {
      static void mapping(IO& IO, OptionalTest &OT) {
         IO.mapOptional("numbers", OT.numbers);
      }
   };

   template <>
   struct MappingTraits<OptionalTestSeq> {
      static void mapping(IO &IO, OptionalTestSeq &OTS) {
         IO.mapOptional("tests", OTS.tests);
      }
   };
   }
}

TEST(YamlIOTest, testSequenceElideTest)
{
   // test that writing out a purely optional structure with its fields set to
   // default followed by other data is properly read back in.
   OptionalTestSeq Seq;
   OptionalTest One, Two, Three, Four;
   int N[] = {1, 2, 3};
   Three.numbers.assign(N, N + 3);
   Seq.tests.push_back(One);
   Seq.tests.push_back(Two);
   Seq.tests.push_back(Three);
   Seq.tests.push_back(Four);

   std::string intermediate;
   {
      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << Seq;
   }

   Input yin(intermediate);
   OptionalTestSeq Seq2;
   yin >> Seq2;

   EXPECT_FALSE(yin.getError());

   EXPECT_EQ(4UL, Seq2.tests.size());

   EXPECT_TRUE(Seq2.tests[0].numbers.empty());
   EXPECT_TRUE(Seq2.tests[1].numbers.empty());

   EXPECT_EQ(1, Seq2.tests[2].numbers[0]);
   EXPECT_EQ(2, Seq2.tests[2].numbers[1]);
   EXPECT_EQ(3, Seq2.tests[2].numbers[2]);

   EXPECT_TRUE(Seq2.tests[3].numbers.empty());
}

TEST(YamlIOTest, testEmptyStringFailsForMapWithRequiredFields)
{
   FooBar doc;
   Input yin("");
   yin >> doc;
   EXPECT_TRUE(!!yin.getError());
}

TEST(YamlIOTest, testEmptyStringSucceedsForMapWithOptionalFields)
{
   OptionalTest doc;
   Input yin("");
   yin >> doc;
   EXPECT_FALSE(yin.getError());
}

TEST(YamlIOTest, testEmptyStringSucceedsForSequence)
{
   std::vector<uint8_t> seq;
   Input yin("", /*Ctxt=*/nullptr, suppress_error_messages);
   yin >> seq;

   EXPECT_FALSE(yin.getError());
   EXPECT_TRUE(seq.empty());
}

struct FlowMap {
   StringRef str1, str2, str3;
   FlowMap(StringRef str1, StringRef str2, StringRef str3)
      : str1(str1), str2(str2), str3(str3) {}
};

struct FlowSeq {
   StringRef str;
   FlowSeq(StringRef S) : str(S) {}
   FlowSeq() = default;
};

namespace polar {
namespace yaml {
template <>
struct MappingTraits<FlowMap> {
   static void mapping(IO &io, FlowMap &fm) {
      io.mapRequired("str1", fm.str1);
      io.mapRequired("str2", fm.str2);
      io.mapRequired("str3", fm.str3);
   }

   static const bool sm_flow = true;
};

template <>
struct ScalarTraits<FlowSeq> {
   static void output(const FlowSeq &value, void*, RawOutStream &out) {
      out << value.str;
   }
   static StringRef input(StringRef scalar, void*, FlowSeq &value) {
      value.str = scalar;
      return "";
   }

   static QuotingType mustQuote(StringRef S) { return QuotingType::None; }
};
}
}

POLAR_YAML_IS_FLOW_SEQUENCE_VECTOR(FlowSeq)

TEST(YamlIOTest, testWrapFlow)
{
   std::string out;
   RawStringOutStream ostr(out);
   FlowMap Map("This is str1", "This is str2", "This is str3");
   std::vector<FlowSeq> Seq;
   Seq.emplace_back("This is str1");
   Seq.emplace_back("This is str2");
   Seq.emplace_back("This is str3");

   {
      // 20 is just bellow the total length of the first mapping field.
      // We should wreap at every element.
      Output yout(ostr, nullptr, 15);

      yout << Map;
      ostr.flush();
      EXPECT_EQ(out,
                "---\n"
                "{ str1: This is str1, \n"
                "  str2: This is str2, \n"
                "  str3: This is str3 }\n"
                "...\n");
      out.clear();

      yout << Seq;
      ostr.flush();
      EXPECT_EQ(out,
                "---\n"
                "[ This is str1, \n"
                "  This is str2, \n"
                "  This is str3 ]\n"
                "...\n");
      out.clear();
   }
   {
      // 25 will allow the second field to be output on the first line.
      Output yout(ostr, nullptr, 25);

      yout << Map;
      ostr.flush();
      EXPECT_EQ(out,
                "---\n"
                "{ str1: This is str1, str2: This is str2, \n"
                "  str3: This is str3 }\n"
                "...\n");
      out.clear();

      yout << Seq;
      ostr.flush();
      EXPECT_EQ(out,
                "---\n"
                "[ This is str1, This is str2, \n"
                "  This is str3 ]\n"
                "...\n");
      out.clear();
   }
   {
      // 0 means no wrapping.
      Output yout(ostr, nullptr, 0);

      yout << Map;
      ostr.flush();
      EXPECT_EQ(out,
                "---\n"
                "{ str1: This is str1, str2: This is str2, str3: This is str3 }\n"
                "...\n");
      out.clear();

      yout << Seq;
      ostr.flush();
      EXPECT_EQ(out,
                "---\n"
                "[ This is str1, This is str2, This is str3 ]\n"
                "...\n");
      out.clear();
   }
}

struct MappingContext
{
   int A = 0;
};
struct SimpleMap {
   int B = 0;
   int C = 0;
};

struct NestedMap {
   NestedMap(MappingContext &Context) : Context(Context) {}
   SimpleMap Simple;
   MappingContext &Context;
};

namespace polar {
namespace yaml {
template <> struct MappingContextTraits<SimpleMap, MappingContext> {
   static void mapping(IO &io, SimpleMap &sm, MappingContext &Context) {
      io.mapRequired("B", sm.B);
      io.mapRequired("C", sm.C);
      ++Context.A;
      io.mapRequired("Context", Context.A);
   }
};

template <> struct MappingTraits<NestedMap> {
   static void mapping(IO &io, NestedMap &nm) {
      io.mapRequired("Simple", nm.Simple, nm.Context);
   }
};
}
}

TEST(YamlIOTest, testMapWithContext)
{
   MappingContext Context;
   NestedMap Nested(Context);
   std::string out;
   RawStringOutStream ostr(out);

   Output yout(ostr, nullptr, 15);

   yout << Nested;
   ostr.flush();
   EXPECT_EQ(1, Context.A);
   EXPECT_EQ("---\n"
             "Simple:          \n"
             "  B:               0\n"
             "  C:               0\n"
             "  Context:         1\n"
             "...\n",
             out);

   out.clear();

   Nested.Simple.B = 2;
   Nested.Simple.C = 3;
   yout << Nested;
   ostr.flush();
   EXPECT_EQ(2, Context.A);
   EXPECT_EQ("---\n"
             "Simple:          \n"
             "  B:               2\n"
             "  C:               3\n"
             "  Context:         2\n"
             "...\n",
             out);
   out.clear();
}

POLAR_YAML_IS_STRING_MAP(int)

TEST(YamlIOTest, testCustomMapping)
{
   std::map<std::string, int> x;
   x["foo"] = 1;
   x["bar"] = 2;

   std::string out;
   RawStringOutStream ostr(out);
   Output xout(ostr, nullptr, 0);

   xout << x;
   ostr.flush();
   EXPECT_EQ("---\n"
             "bar:             2\n"
             "foo:             1\n"
             "...\n",
             out);

   Input yin(out);
   std::map<std::string, int> y;
   yin >> y;
   EXPECT_EQ(2ul, y.size());
   EXPECT_EQ(1, y["foo"]);
   EXPECT_EQ(2, y["bar"]);
}

POLAR_YAML_IS_STRING_MAP(FooBar)

TEST(YamlIOTest, testCustomMappingStruct)
{
   std::map<std::string, FooBar> x;
   x["foo"].foo = 1;
   x["foo"].bar = 2;
   x["bar"].foo = 3;
   x["bar"].bar = 4;

   std::string out;
   RawStringOutStream ostr(out);
   Output xout(ostr, nullptr, 0);

   xout << x;
   ostr.flush();
   EXPECT_EQ("---\n"
             "bar:             \n"
             "  foo:             3\n"
             "  bar:             4\n"
             "foo:             \n"
             "  foo:             1\n"
             "  bar:             2\n"
             "...\n",
             out);

   Input yin(out);
   std::map<std::string, FooBar> y;
   yin >> y;
   EXPECT_EQ(2ul, y.size());
   EXPECT_EQ(1, y["foo"].foo);
   EXPECT_EQ(2, y["foo"].bar);
   EXPECT_EQ(3, y["bar"].foo);
   EXPECT_EQ(4, y["bar"].bar);
}

static void testEscaped(StringRef Input, StringRef Expected)
{
   std::string out;
   RawStringOutStream ostr(out);
   Output xout(ostr, nullptr, 0);

   polar::yaml::EmptyContext Ctx;
   yamlize(xout, Input, true, Ctx);
   ostr.flush();
   // Make a separate StringRef so we get nice byte-by-byte output.
   StringRef Got(out);
   EXPECT_EQ(Expected, Got);
}

TEST(YamlIOTest, testEscaped)
{
   // Single quote
   testEscaped("@abc@", "'@abc@'");
   // No quote
   testEscaped("abc", "abc");
   // Forward slash quoted
   testEscaped("abc/", "'abc/'");
   // Double quote non-printable
   testEscaped("\01@abc@", "\"\\x01@abc@\"");
   // Double quote inside single quote
   testEscaped("abc\"fdf", "'abc\"fdf'");
   // Double quote inside double quote
   testEscaped("\01bc\"fdf", "\"\\x01bc\\\"fdf\"");
   // Single quote inside single quote
   testEscaped("abc'fdf", "'abc''fdf'");
   // UTF8
   testEscaped("/*параметр*/", "\"/*параметр*/\"");
   // UTF8 with single quote inside double quote
   testEscaped("parameter 'параметр' is unused",
               "\"parameter 'параметр' is unused\"");

   // String with embedded non-printable multibyte UTF-8 sequence (U+200B
   // zero-width space). The thing to test here is that we emit a
   // unicode-scalar level escape like \uNNNN (at the YAML level), and don't
   // just pass the UTF-8 byte sequence through as with quoted printables.
   {
      const unsigned char foobar[10] = {'f', 'o', 'o',
                                        0xE2, 0x80, 0x8B, // UTF-8 of U+200B
                                        'b', 'a', 'r',
                                        0x0};
      testEscaped((char const *)foobar, "\"foo\\u200Bbar\"");
   }
}


TEST(YamlIOTest, testNumeric)
{
   EXPECT_TRUE(is_numeric(".inf"));
   EXPECT_TRUE(is_numeric(".INF"));
   EXPECT_TRUE(is_numeric(".Inf"));
   EXPECT_TRUE(is_numeric("-.inf"));
   EXPECT_TRUE(is_numeric("+.inf"));

   EXPECT_TRUE(is_numeric(".nan"));
   EXPECT_TRUE(is_numeric(".NaN"));
   EXPECT_TRUE(is_numeric(".NAN"));

   EXPECT_TRUE(is_numeric("0"));
   EXPECT_TRUE(is_numeric("0."));
   EXPECT_TRUE(is_numeric("0.0"));
   EXPECT_TRUE(is_numeric("-0.0"));
   EXPECT_TRUE(is_numeric("+0.0"));

   EXPECT_TRUE(is_numeric("12345"));
   EXPECT_TRUE(is_numeric("012345"));
   EXPECT_TRUE(is_numeric("+12.0"));
   EXPECT_TRUE(is_numeric(".5"));
   EXPECT_TRUE(is_numeric("+.5"));
   EXPECT_TRUE(is_numeric("-1.0"));

   EXPECT_TRUE(is_numeric("2.3e4"));
   EXPECT_TRUE(is_numeric("-2E+05"));
   EXPECT_TRUE(is_numeric("+12e03"));
   EXPECT_TRUE(is_numeric("6.8523015e+5"));

   EXPECT_TRUE(is_numeric("1.e+1"));
   EXPECT_TRUE(is_numeric(".0e+1"));

   EXPECT_TRUE(is_numeric("0x2aF3"));
   EXPECT_TRUE(is_numeric("0o01234567"));

   EXPECT_FALSE(is_numeric("not a number"));
   EXPECT_FALSE(is_numeric("."));
   EXPECT_FALSE(is_numeric(".e+1"));
   EXPECT_FALSE(is_numeric(".1e"));
   EXPECT_FALSE(is_numeric(".1e+"));
   EXPECT_FALSE(is_numeric(".1e++1"));

   EXPECT_FALSE(is_numeric("ABCD"));
   EXPECT_FALSE(is_numeric("+0x2AF3"));
   EXPECT_FALSE(is_numeric("-0x2AF3"));
   EXPECT_FALSE(is_numeric("0x2AF3Z"));
   EXPECT_FALSE(is_numeric("0o012345678"));
   EXPECT_FALSE(is_numeric("0xZ"));
   EXPECT_FALSE(is_numeric("-0o012345678"));
   EXPECT_FALSE(is_numeric("000003A8229434B839616A25C16B0291F77A438B"));

   EXPECT_FALSE(is_numeric(""));
   EXPECT_FALSE(is_numeric("."));
   EXPECT_FALSE(is_numeric(".e+1"));
   EXPECT_FALSE(is_numeric(".e+"));
   EXPECT_FALSE(is_numeric(".e"));
   EXPECT_FALSE(is_numeric("e1"));

   // Deprecated formats: as for YAML 1.2 specification, the following are not
   // valid numbers anymore:
   //
   // * Sexagecimal numbers
   // * Decimal numbers with comma s the delimiter
   // * "inf", "nan" without '.' prefix
   EXPECT_FALSE(is_numeric("3:25:45"));
   EXPECT_FALSE(is_numeric("+12,345"));
   EXPECT_FALSE(is_numeric("-inf"));
   EXPECT_FALSE(is_numeric("1,230.15"));
}

//===----------------------------------------------------------------------===//
//  Test PolymorphicTraits and TaggedScalarTraits
//===----------------------------------------------------------------------===//

struct Poly {
   enum NodeKind {
      NK_Scalar,
      NK_Seq,
      NK_Map,
   } Kind;

   Poly(NodeKind Kind) : Kind(Kind) {}

   virtual ~Poly() = default;

   NodeKind getKind() const { return Kind; }
};

struct Scalar : Poly {
   enum ScalarKind {
      SK_Unknown,
      SK_Double,
      SK_Bool,
   } SKind;

   union {
      double DoubleValue;
      bool BoolValue;
   };

   Scalar() : Poly(NK_Scalar), SKind(SK_Unknown) {}
   Scalar(double DoubleValue)
      : Poly(NK_Scalar), SKind(SK_Double), DoubleValue(DoubleValue) {}
   Scalar(bool BoolValue)
      : Poly(NK_Scalar), SKind(SK_Bool), BoolValue(BoolValue) {}

   static bool classOf(const Poly *N) { return N->getKind() == NK_Scalar; }
};

struct Seq : Poly, std::vector<std::unique_ptr<Poly>> {
   Seq() : Poly(NK_Seq) {}

   static bool classOf(const Poly *N) { return N->getKind() == NK_Seq; }
};

struct Map : Poly, StringMap<std::unique_ptr<Poly>>
{
   Map() : Poly(NK_Map) {}

   static bool classOf(const Poly *N) { return N->getKind() == NK_Map; }
};

namespace polar {
namespace yaml {

template <> struct PolymorphicTraits<std::unique_ptr<Poly>> {
   static NodeKind getKind(const std::unique_ptr<Poly> &N) {
      if (isa<Scalar>(*N))
         return NodeKind::Scalar;
      if (isa<Seq>(*N))
         return NodeKind::Sequence;
      if (isa<Map>(*N))
         return NodeKind::Map;
      polar_unreachable("unsupported node type");
   }

   static Scalar &getAsScalar(std::unique_ptr<Poly> &N) {
      if (!N || !isa<Scalar>(*N))
         N = std::make_unique<Scalar>();
      return *cast<Scalar>(N.get());
   }

   static Seq &getAsSequence(std::unique_ptr<Poly> &N) {
      if (!N || !isa<Seq>(*N))
         N = std::make_unique<Seq>();
      return *cast<Seq>(N.get());
   }

   static Map &getAsMap(std::unique_ptr<Poly> &N) {
      if (!N || !isa<Map>(*N))
         N = std::make_unique<Map>();
      return *cast<Map>(N.get());
   }
};

template <> struct TaggedScalarTraits<Scalar>
{
   static void output(const Scalar &S, void *Ctxt, RawOutStream &ScalarOS,
                      RawOutStream &TagOS)
   {
      switch (S.SKind) {
      case Scalar::SK_Unknown:
         report_fatal_error("output unknown scalar");
         break;
      case Scalar::SK_Double:
         TagOS << "!double";
         ScalarTraits<double>::output(S.DoubleValue, Ctxt, ScalarOS);
         break;
      case Scalar::SK_Bool:
         TagOS << "!bool";
         ScalarTraits<bool>::output(S.BoolValue, Ctxt, ScalarOS);
         break;
      }
   }

   static StringRef input(StringRef ScalarStr, StringRef Tag, void *Ctxt,
                          Scalar &S)
   {
      S.SKind = StringSwitch<Scalar::ScalarKind>(Tag)
            .cond("!double", Scalar::SK_Double)
            .cond("!bool", Scalar::SK_Bool)
            .defaultCond(Scalar::SK_Unknown);
      switch (S.SKind) {
      case Scalar::SK_Unknown:
         return StringRef("unknown scalar tag");
      case Scalar::SK_Double:
         return ScalarTraits<double>::input(ScalarStr, Ctxt, S.DoubleValue);
      case Scalar::SK_Bool:
         return ScalarTraits<bool>::input(ScalarStr, Ctxt, S.BoolValue);
      }
      polar_unreachable("unknown scalar kind");
   }

   static QuotingType mustQuote(const Scalar &S, StringRef Str)
   {
      switch (S.SKind) {
      case Scalar::SK_Unknown:
         report_fatal_error("quote unknown scalar");
      case Scalar::SK_Double:
         return ScalarTraits<double>::mustQuote(Str);
      case Scalar::SK_Bool:
         return ScalarTraits<bool>::mustQuote(Str);
      }
      polar_unreachable("unknown scalar kind");
   }
};

template <> struct CustomMappingTraits<Map>
{
   static void inputOne(IO &IO, StringRef Key, Map &M)
   {
      IO.mapRequired(Key.getStr().c_str(), M[Key]);
   }

   static void output(IO &IO, Map &M)
   {
      for (auto &N : M)
         IO.mapRequired(N.getKey().getStr().c_str(), N.getValue());
   }
};

template <> struct SequenceTraits<Seq>
{
   static size_t size(IO &IO, Seq &A) { return A.size(); }

   static std::unique_ptr<Poly> &element(IO &IO, Seq &A, size_t Index) {
      if (Index >= A.size())
         A.resize(Index + 1);
      return A[Index];
   }
};

} // namespace yaml
} // namespace polar

TEST(YamlIOTest, testReadWritePolymorphicScalar)
{
   std::string intermediate;
   std::unique_ptr<Poly> node = std::make_unique<Scalar>(true);

   RawStringOutStream ostr(intermediate);
   Output yout(ostr);
#ifdef GTEST_HAS_DEATH_TEST
#ifndef NDEBUG
   EXPECT_DEATH(yout << node, "plain scalar documents are not supported");
#endif
#endif
}

TEST(YamlIOTest, testReadWritePolymorphicSeq)
{
   std::string intermediate;
   {
      auto seq = std::make_unique<Seq>();
      seq->push_back(std::make_unique<Scalar>(true));
      seq->push_back(std::make_unique<Scalar>(1.0));
      auto node = unique_dyn_cast<Poly>(seq);

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << node;
   }
   {
      Input yin(intermediate);
      std::unique_ptr<Poly> node;
      yin >> node;

      EXPECT_FALSE(yin.getError());
      auto seq = dyn_cast<Seq>(node.get());
      ASSERT_TRUE(seq);
      ASSERT_EQ(seq->size(), 2u);
      auto first = dyn_cast<Scalar>((*seq)[0].get());
      ASSERT_TRUE(first);
      EXPECT_EQ(first->SKind, Scalar::SK_Bool);
      EXPECT_TRUE(first->BoolValue);
      auto second = dyn_cast<Scalar>((*seq)[1].get());
      ASSERT_TRUE(second);
      EXPECT_EQ(second->SKind, Scalar::SK_Double);
      EXPECT_EQ(second->DoubleValue, 1.0);
   }
}

TEST(YamlIOTest, TestReadWritePolymorphicMap)
{
   std::string intermediate;
   {
      auto map = std::make_unique<Map>();
      (*map)["foo"] = std::make_unique<Scalar>(false);
      (*map)["bar"] = std::make_unique<Scalar>(2.0);
      std::unique_ptr<Poly> node = unique_dyn_cast<Poly>(map);

      RawStringOutStream ostr(intermediate);
      Output yout(ostr);
      yout << node;
   }
   {
      Input yin(intermediate);
      std::unique_ptr<Poly> node;
      yin >> node;

      EXPECT_FALSE(yin.getError());
      auto map = dyn_cast<Map>(node.get());
      ASSERT_TRUE(map);
      auto foo = dyn_cast<Scalar>((*map)["foo"].get());
      ASSERT_TRUE(foo);
      EXPECT_EQ(foo->SKind, Scalar::SK_Bool);
      EXPECT_FALSE(foo->BoolValue);
      auto bar = dyn_cast<Scalar>((*map)["bar"].get());
      ASSERT_TRUE(bar);
      EXPECT_EQ(bar->SKind, Scalar::SK_Double);
      EXPECT_EQ(bar->DoubleValue, 2.0);
   }
}

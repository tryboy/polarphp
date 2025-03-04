// This source file is part of the polarphp.org open source project
//
// Copyright (c) 2017 - 2019 polarphp software foundation
// Copyright (c) 2017 - 2019 zzu_softboy <zzu_softboy@163.com>
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://polarphp.org/LICENSE.txt for license information
// See https://polarphp.org/CONTRIBUTORS.txt for the list of polarphp project authors
//
// Created by polarboy on 2018/07/12.

#include "polarphp/utils/Casting.h"
#include "polarphp/utils/Debug.h"
#include "polarphp/utils/RawOutStream.h"
#include "gtest/gtest.h"
#include <cstdlib>

namespace polar {
namespace utils {

// Used to test illegal cast. If a cast doesn't match any of the "real" ones,
// it will match this one.
struct IllegalCast;
template <typename T> IllegalCast *cast(...) { return nullptr; }

// set up two example classes
// with conversion facility
//
struct bar
{
   bar() {}
   struct foo *baz();
   struct foo *caz();
   struct foo *daz();
   struct foo *naz();
private:
   bar(const bar &);
};
struct foo
{
   void ext() const;
   /*  static bool classOf(const bar *X) {
    cerr << "Classof: " << X << "\n";
    return true;
    }*/
};

struct base
{
   virtual ~base() {}
};

struct derived : public base
{
   static bool classOf(const base *B) { return true; }
};

template <> struct IsaImpl<foo, bar>
{
   static inline bool doit(const bar &Val) {
      debug_stream() << "Classof: " << &Val << "\n";
      return true;
   }
};

template <typename T> struct IsaImpl<foo, T>
{
   static inline bool doit(const T &Val) { return false; }
};

foo *bar::baz()
{
   return cast<foo>(this);
}

foo *bar::caz()
{
   return cast_or_null<foo>(this);
}

foo *bar::daz()
{
   return dyn_cast<foo>(this);
}

foo *bar::naz()
{
   return dyn_cast_or_null<foo>(this);
}


bar *fub();

template <>
struct SimplifyType<foo>
{
   typedef int SimpleType;
   static SimpleType getSimplifiedValue(foo &Val) { return 0; }
};

} // utils
} // End polar namespace

using namespace polar::basic;
using namespace polar::utils;

// Test that a regular class behaves as expected.
static_assert(std::is_same<SimplifyType<foo>::SimpleType, int>::value,
              "Unexpected SimplifyType result!");
static_assert(std::is_same<SimplifyType<foo *>::SimpleType, foo *>::value,
              "Unexpected SimplifyType result!");

namespace {

const foo *null_foo = nullptr;

bar B;
extern bar &B1;
bar &B1 = B;
extern const bar *B2;
// test various configurations of const
const bar &B3 = B1;
const bar *const B4 = B2;

TEST(CastingTest, testIsa) {
   EXPECT_TRUE(isa<foo>(B1));
   EXPECT_TRUE(isa<foo>(B2));
   EXPECT_TRUE(isa<foo>(B3));
   EXPECT_TRUE(isa<foo>(B4));
}

TEST(CastingTest, testCast) {
   foo &F1 = cast<foo>(B1);
   EXPECT_NE(&F1, null_foo);
   const foo *F3 = cast<foo>(B2);
   EXPECT_NE(F3, null_foo);
   const foo *F4 = cast<foo>(B2);
   EXPECT_NE(F4, null_foo);
   const foo &F5 = cast<foo>(B3);
   EXPECT_NE(&F5, null_foo);
   const foo *F6 = cast<foo>(B4);
   EXPECT_NE(F6, null_foo);
   // Can't pass null pointer to cast<>.
   // foo *F7 = cast<foo>(fub());
   // EXPECT_EQ(F7, null_foo);
   foo *F8 = B1.baz();
   EXPECT_NE(F8, null_foo);

   std::unique_ptr<const bar> BP(B2);
   auto FP = cast<foo>(std::move(BP));
   static_assert(std::is_same<std::unique_ptr<const foo>, decltype(FP)>::value,
                 "Incorrect deduced return type!");
   EXPECT_NE(FP.get(), null_foo);
   FP.release();
}

TEST(CastingTest, testCastOrNull) {
   const foo *F11 = cast_or_null<foo>(B2);
   EXPECT_NE(F11, null_foo);
   const foo *F12 = cast_or_null<foo>(B2);
   EXPECT_NE(F12, null_foo);
   const foo *F13 = cast_or_null<foo>(B4);
   EXPECT_NE(F13, null_foo);
   const foo *F14 = cast_or_null<foo>(fub());  // Shouldn't print.
   EXPECT_EQ(F14, null_foo);
   foo *F15 = B1.caz();
   EXPECT_NE(F15, null_foo);

   std::unique_ptr<const bar> BP(fub());
   auto FP = cast_or_null<foo>(std::move(BP));
   EXPECT_EQ(FP.get(), null_foo);
}

TEST(CastingTest, testDynCast) {
   const foo *F1 = dyn_cast<foo>(B2);
   EXPECT_NE(F1, null_foo);
   const foo *F2 = dyn_cast<foo>(B2);
   EXPECT_NE(F2, null_foo);
   const foo *F3 = dyn_cast<foo>(B4);
   EXPECT_NE(F3, null_foo);
   // Can't pass null pointer to dyn_cast<>.
   // foo *F4 = dyn_cast<foo>(fub());
   // EXPECT_EQ(F4, null_foo);
   foo *F5 = B1.daz();
   EXPECT_NE(F5, null_foo);
}

TEST(CastingTest, testDynCastOrNull) {
   const foo *F1 = dyn_cast_or_null<foo>(B2);
   EXPECT_NE(F1, null_foo);
   const foo *F2 = dyn_cast_or_null<foo>(B2);
   EXPECT_NE(F2, null_foo);
   const foo *F3 = dyn_cast_or_null<foo>(B4);
   EXPECT_NE(F3, null_foo);
   foo *F4 = dyn_cast_or_null<foo>(fub());
   EXPECT_EQ(F4, null_foo);
   foo *F5 = B1.naz();
   EXPECT_NE(F5, null_foo);
}

std::unique_ptr<derived> newd() { return std::make_unique<derived>(); }
std::unique_ptr<base> newb() { return std::make_unique<derived>(); }

TEST(CastingTest, testUniqueDynCast) {
   derived *OrigD = nullptr;
   auto D = std::make_unique<derived>();
   OrigD = D.get();

   // Converting from D to itself is valid, it should return a new unique_ptr
   // and the old one should become nullptr.
   auto NewD = unique_dyn_cast<derived>(D);
   ASSERT_EQ(OrigD, NewD.get());
   ASSERT_EQ(nullptr, D);

   // Converting from D to B is valid, B should have a value and D should be
   // nullptr.
   auto B = unique_dyn_cast<base>(NewD);
   ASSERT_EQ(OrigD, B.get());
   ASSERT_EQ(nullptr, NewD);

   // Converting from B to itself is valid, it should return a new unique_ptr
   // and the old one should become nullptr.
   auto NewB = unique_dyn_cast<base>(B);
   ASSERT_EQ(OrigD, NewB.get());
   ASSERT_EQ(nullptr, B);

   // Converting from B to D is valid, D should have a value and B should be
   // nullptr;
   D = unique_dyn_cast<derived>(NewB);
   ASSERT_EQ(OrigD, D.get());
   ASSERT_EQ(nullptr, NewB);

   // Converting between unrelated types should fail.  The original value should
   // remain unchanged and it should return nullptr.
   auto F = unique_dyn_cast<foo>(D);
   ASSERT_EQ(nullptr, F);
   ASSERT_EQ(OrigD, D.get());

   // All of the above should also hold for temporaries.
   auto D2 = unique_dyn_cast<derived>(newd());
   EXPECT_NE(nullptr, D2);

   auto B2 = unique_dyn_cast<derived>(newb());
   EXPECT_NE(nullptr, B2);

   auto B3 = unique_dyn_cast<base>(newb());
   EXPECT_NE(nullptr, B3);

   auto F2 = unique_dyn_cast<foo>(newb());
   EXPECT_EQ(nullptr, F2);
}

// These lines are errors...
//foo *F20 = cast<foo>(B2);  // Yields const foo*
//foo &F21 = cast<foo>(B3);  // Yields const foo&
//foo *F22 = cast<foo>(B4);  // Yields const foo*
//foo &F23 = cast_or_null<foo>(B1);
//const foo &F24 = cast_or_null<foo>(B3);

const bar *B2 = &B;
}  // anonymous namespace

bar *polar::utils::fub() { return nullptr; }

namespace {
namespace inferred_upcasting {
// This test case verifies correct behavior of inferred upcasts when the
// types are statically known to be OK to upcast. This is the case when,
// for example, Derived inherits from Base, and we do `isa<Base>(Derived)`.

// Note: This test will actually fail to compile without inferred
// upcasting.

class Base {
public:
   // No classOf. We are testing that the upcast is inferred.
   Base() {}
};

class Derived : public Base {
public:
   Derived() {}
};

// Even with no explicit classOf() in Base, we should still be able to cast
// Derived to its base class.
TEST(CastingTest, testUpcastIsInferred) {
   Derived D;
   EXPECT_TRUE(isa<Base>(D));
   Base *BP = dyn_cast<Base>(&D);
   EXPECT_TRUE(BP != nullptr);
}


// This test verifies that the inferred upcast takes precedence over an
// explicitly written one. This is important because it verifies that the
// dynamic check gets optimized away.
class UseInferredUpcast {
public:
   int Dummy;
   static bool classOf(const UseInferredUpcast *) {
      return false;
   }
};

TEST(CastingTest, testInferredUpcastTakesPrecedence) {
   UseInferredUpcast UIU;
   // Since the explicit classOf() returns false, this will fail if the
   // explicit one is used.
   EXPECT_TRUE(isa<UseInferredUpcast>(&UIU));
}

} // end namespace inferred_upcasting
} // end anonymous namespace
// Test that we reject casts of temporaries (and so the illegal cast gets used).
namespace TemporaryCast {
struct pod {};
IllegalCast *testIllegalCast() { return cast<foo>(pod()); }
}

namespace {
namespace pointer_wrappers {

struct Base {
   bool IsDerived;
   Base(bool IsDerived = false) : IsDerived(IsDerived) {}
};

struct Derived : Base {
   Derived() : Base(true) {}
   static bool classOf(const Base *B) { return B->IsDerived; }
};

class PTy {
   Base *B;
public:
   PTy(Base *B) : B(B) {}
   explicit operator bool() const { return get(); }
   Base *get() const { return B; }
};

} // end namespace pointer_wrappers
} // end namespace

namespace polar {
namespace utils {

template <> struct SimplifyType<pointer_wrappers::PTy>
{
   typedef pointer_wrappers::Base *SimpleType;
   static SimpleType getSimplifiedValue(pointer_wrappers::PTy &P) {
      return P.get();
   }
};

template <> struct SimplifyType<const pointer_wrappers::PTy>
{
   typedef pointer_wrappers::Base *SimpleType;
   static SimpleType getSimplifiedValue(const pointer_wrappers::PTy &P) {
      return P.get();
   }
};

} // utils
} // end namespace llvm

namespace {
namespace pointer_wrappers {

// Some objects.
pointer_wrappers::Base B;
pointer_wrappers::Derived D;

// Mutable "smart" pointers.
pointer_wrappers::PTy MN(nullptr);
pointer_wrappers::PTy MB(&B);
pointer_wrappers::PTy MD(&D);

// Const "smart" pointers.
const pointer_wrappers::PTy CN(nullptr);
const pointer_wrappers::PTy CB(&B);
const pointer_wrappers::PTy CD(&D);

TEST(CastingTest, testSmartIsa)
{
   EXPECT_TRUE(!isa<pointer_wrappers::Derived>(MB));
   EXPECT_TRUE(!isa<pointer_wrappers::Derived>(CB));
   EXPECT_TRUE(isa<pointer_wrappers::Derived>(MD));
   EXPECT_TRUE(isa<pointer_wrappers::Derived>(CD));
}

TEST(CastingTest, testSmartCast)
{
   EXPECT_TRUE(cast<pointer_wrappers::Derived>(MD) == &D);
   EXPECT_TRUE(cast<pointer_wrappers::Derived>(CD) == &D);
}

TEST(CastingTest, testSmartCastOrNull)
{
   EXPECT_TRUE(cast_or_null<pointer_wrappers::Derived>(MN) == nullptr);
   EXPECT_TRUE(cast_or_null<pointer_wrappers::Derived>(CN) == nullptr);
   EXPECT_TRUE(cast_or_null<pointer_wrappers::Derived>(MD) == &D);
   EXPECT_TRUE(cast_or_null<pointer_wrappers::Derived>(CD) == &D);
}

TEST(CastingTest, testSmartDynCast)
{
   EXPECT_TRUE(dyn_cast<pointer_wrappers::Derived>(MB) == nullptr);
   EXPECT_TRUE(dyn_cast<pointer_wrappers::Derived>(CB) == nullptr);
   EXPECT_TRUE(dyn_cast<pointer_wrappers::Derived>(MD) == &D);
   EXPECT_TRUE(dyn_cast<pointer_wrappers::Derived>(CD) == &D);
}

TEST(CastingTest, testSmartDynCastOrNull)
{
   EXPECT_TRUE(dyn_cast_or_null<pointer_wrappers::Derived>(MN) == nullptr);
   EXPECT_TRUE(dyn_cast_or_null<pointer_wrappers::Derived>(CN) == nullptr);
   EXPECT_TRUE(dyn_cast_or_null<pointer_wrappers::Derived>(MB) == nullptr);
   EXPECT_TRUE(dyn_cast_or_null<pointer_wrappers::Derived>(CB) == nullptr);
   EXPECT_TRUE(dyn_cast_or_null<pointer_wrappers::Derived>(MD) == &D);
   EXPECT_TRUE(dyn_cast_or_null<pointer_wrappers::Derived>(CD) == &D);
}

} // end namespace pointer_wrappers

} // end namespace

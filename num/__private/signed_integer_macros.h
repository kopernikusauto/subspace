// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <stdint.h>

#include <compare>

#include "assertions/check.h"
#include "assertions/endian.h"
#include "containers/array.h"
#include "marker/unsafe.h"
#include "num/__private/int_log10.h"
#include "num/__private/intrinsics.h"
#include "num/u32.h"
#include "option/option.h"
#include "tuple/tuple.h"

#define _sus__signed_constants(T, Max)                               \
  static constexpr auto MIN_PRIMITIVE =                              \
      __private::min_value<primitive_type>();                        \
  static constexpr auto MAX_PRIMITIVE =                              \
      __private::max_value<primitive_type>();                        \
  static constexpr inline T MIN() noexcept { return MIN_PRIMITIVE; } \
  static constexpr inline T MAX() noexcept { return MAX_PRIMITIVE; } \
  static constexpr inline u32 BITS() noexcept {                      \
    return __private::num_bits<primitive_type>();                    \
  }                                                                  \
  static_assert(true)

#define _sus__signed_impl(T, Bytes, LargerT, UnsignedT, UnsignedSusT) \
  _sus__signed_from(T);                                               \
  _sus__signed_integer_comparison(T);                                 \
  _sus__signed_unary_ops(T, UnsignedT);                               \
  _sus__signed_binary_logic_ops(T);                                   \
  _sus__signed_binary_bit_ops(T, UnsignedT);                          \
  _sus__signed_mutable_logic_ops(T);                                  \
  _sus__signed_mutable_bit_ops(T, UnsignedT);                         \
  _sus__signed_abs(T, UnsignedT, UnsignedSusT);                       \
  _sus__signed_add(T, UnsignedT);                                     \
  _sus__signed_div(T);                                                \
  _sus__signed_mul(T, LargerT);                                       \
  _sus__signed_neg(T);                                                \
  _sus__signed_rem(T);                                                \
  _sus__signed_euclid(T);                                             \
  _sus__signed_shift(T);                                              \
  _sus__signed_sub(T, UnsignedT);                                     \
  _sus__signed_bits(T, UnsignedT);                                    \
  _sus__signed_pow(T);                                                \
  _sus__signed_log(T, UnsignedT);                                     \
  _sus__signed_endian(T, UnsignedT, Bytes)

#define _sus__signed_from(T)                                                \
  /** Constructs a ##T## from a signed integer type (i8, i16, i32, etc).    \
   *                                                                        \
   * # Panics                                                               \
   * The function will panic if the input value is out of range for ##T##.  \
   */                                                                       \
  template <Signed S>                                                       \
  static constexpr T from(S s) noexcept {                                   \
    if constexpr (MIN_PRIMITIVE > S::MIN_PRIMITIVE)                         \
      ::sus::check(s.primitive_value >= MIN_PRIMITIVE);                     \
    if constexpr (MAX_PRIMITIVE < S::MAX_PRIMITIVE)                         \
      ::sus::check(s.primitive_value <= MAX_PRIMITIVE);                     \
    return T(static_cast<primitive_type>(s.primitive_value));               \
  }                                                                         \
                                                                            \
  /** Constructs a ##T## from an unsigned integer type (u8, u16, u32, etc). \
   *                                                                        \
   * # Panics                                                               \
   * The function will panic if the input value is out of range for ##T##.  \
   */                                                                       \
  template <Unsigned U>                                                     \
  static constexpr T from(U u) noexcept {                                   \
    constexpr auto umax =                                                   \
        static_cast<decltype(U::primitive_value)>(MAX_PRIMITIVE);           \
    if constexpr (umax < U::MAX_PRIMITIVE) {                                \
      ::sus::check(u.primitive_value <= umax);                              \
    }                                                                       \
    return T(static_cast<primitive_type>(u.primitive_value));               \
  }                                                                         \
  static_assert(true)

#define _sus__signed_integer_comparison(T)                                     \
  /** Returns true if the current value is positive and false if the number is \
   * zero or negative.                                                         \
   */                                                                          \
  constexpr bool is_negative() const& noexcept {                               \
    return primitive_value < primitive_type{0};                                \
  }                                                                            \
  /** Returns true if the current value is negative and false if the number is \
   * zero or positive.                                                         \
   */                                                                          \
  constexpr bool is_positive() const& noexcept {                               \
    return primitive_value > primitive_type{0};                                \
  }                                                                            \
                                                                               \
  /** Returns a number representing sign of the current value.                 \
   *                                                                           \
   * - 0 if the number is zero                                                 \
   * - 1 if the number is positive                                             \
   * - -1 if the number is negative                                            \
   */                                                                          \
  constexpr T signum() const& noexcept {                                       \
    if (primitive_value < primitive_type{0})                                   \
      return primitive_type{-1};                                               \
    else                                                                       \
      return primitive_type{primitive_value != primitive_type{0}};             \
  }                                                                            \
                                                                               \
  /** sus::concepts::Eq<##T##> trait. */                                       \
  friend constexpr inline bool operator==(const T& l, const T& r) noexcept {   \
    return (l.primitive_value <=> r.primitive_value) == 0;                     \
  }                                                                            \
  /** sus::concepts::Ord<##T##> trait. */                                      \
  friend constexpr inline auto operator<=>(const T& l, const T& r) noexcept {  \
    return l.primitive_value <=> r.primitive_value;                            \
  }                                                                            \
  static_assert(true)

#define _sus__signed_unary_ops(T, UnsignedT)             \
  /** sus::concepts::Neg trait. */                       \
  constexpr inline T operator-() const& noexcept {       \
    /* TODO: Allow opting out of all overflow checks? */ \
    ::sus::check(primitive_value != MIN_PRIMITIVE);      \
    return -primitive_value;                             \
  }                                                      \
  /** sus::concepts::BitNot trait. */                    \
  constexpr inline T operator~() const& noexcept {       \
    return static_cast<primitive_type>(                  \
        ~static_cast<UnsignedT>(primitive_value));       \
  }                                                      \
  static_assert(true)

#define _sus__signed_binary_logic_ops(T)                                    \
  /** sus::concepts::Add<##T##> trait. */                                   \
  friend constexpr inline T operator+(const T& l, const T& r) noexcept {    \
    auto out =                                                              \
        __private::add_with_overflow(l.primitive_value, r.primitive_value); \
    /* TODO: Allow opting out of all overflow checks? */                    \
    ::sus::check(!out.overflow);                                            \
    return out.value;                                                       \
  }                                                                         \
  /** sus::concepts::Sub<##T##> trait. */                                   \
  friend constexpr inline T operator-(const T& l, const T& r) noexcept {    \
    auto out =                                                              \
        __private::sub_with_overflow(l.primitive_value, r.primitive_value); \
    /* TODO: Allow opting out of all overflow checks? */                    \
    ::sus::check(!out.overflow);                                            \
    return out.value;                                                       \
  }                                                                         \
  /** sus::concepts::Mul<##T##> trait. */                                   \
  friend constexpr inline T operator*(const T& l, const T& r) noexcept {    \
    auto out =                                                              \
        __private::mul_with_overflow(l.primitive_value, r.primitive_value); \
    /* TODO: Allow opting out of all overflow checks? */                    \
    ::sus::check(!out.overflow);                                            \
    return out.value;                                                       \
  }                                                                         \
  /** sus::concepts::Div<##T##> trait. */                                   \
  friend constexpr inline T operator/(const T& l, const T& r) noexcept {    \
    /* TODO: Allow opting out of all overflow checks? */                    \
    ::sus::check(r.primitive_value != primitive_type{0});                   \
    /* TODO: Allow opting out of all overflow checks? */                    \
    ::sus::check(l.primitive_value != MIN_PRIMITIVE ||                      \
                 r.primitive_value != primitive_type{-1});                  \
    return l.primitive_value / r.primitive_value;                           \
  }                                                                         \
  /** sus::concepts::Rem<##T##> trait. */                                   \
  friend constexpr inline T operator%(const T& l, const T& r) noexcept {    \
    /* TODO: Allow opting out of all overflow checks? */                    \
    ::sus::check(r.primitive_value != primitive_type{0});                   \
    /* TODO: Allow opting out of all overflow checks? */                    \
    ::sus::check(l.primitive_value != MIN_PRIMITIVE ||                      \
                 r.primitive_value != primitive_type{-1});                  \
    return l.primitive_value % r.primitive_value;                           \
  }                                                                         \
  static_assert(true)

#define _sus__signed_binary_bit_ops(T, UnsignedT)                           \
  /** sus::concepts::BitAnd<##T##> trait. */                                \
  friend constexpr inline T operator&(const T& l, const T& r) noexcept {    \
    return l.primitive_value & r.primitive_value;                           \
  }                                                                         \
  /** sus::concepts::BitOr<##T##> trait. */                                 \
  friend constexpr inline T operator|(const T& l, const T& r) noexcept {    \
    return l.primitive_value | r.primitive_value;                           \
  }                                                                         \
  /** sus::concepts::BitXor<##T##> trait. */                                \
  friend constexpr inline T operator^(const T& l, const T& r) noexcept {    \
    return l.primitive_value ^ r.primitive_value;                           \
  }                                                                         \
  /** sus::concepts::Shl trait. */                                          \
  friend constexpr inline T operator<<(const T& l, const u32& r) noexcept { \
    /* TODO: Allow opting out of all overflow checks? */                    \
    ::sus::check(r < BITS());                                               \
    return static_cast<primitive_type>(                                     \
        static_cast<UnsignedT>(l.primitive_value) << r.primitive_value);    \
  }                                                                         \
  /** sus::concepts::Shr trait. */                                          \
  friend constexpr inline T operator>>(const T& l, const u32& r) noexcept { \
    /* TODO: Allow opting out of all overflow checks? */                    \
    ::sus::check(r < BITS());                                               \
    return static_cast<primitive_type>(                                     \
        static_cast<UnsignedT>(l.primitive_value) >> r.primitive_value);    \
  }                                                                         \
  static_assert(true)

#define _sus__signed_mutable_logic_ops(T)                                 \
  /** sus::concepts::AddAssign<##T##> trait. */                           \
  constexpr inline void operator+=(T r)& noexcept {                       \
    auto out =                                                            \
        __private::add_with_overflow(primitive_value, r.primitive_value); \
    /* TODO: Allow opting out of all overflow checks? */                  \
    ::sus::check(!out.overflow);                                          \
    primitive_value = out.value;                                          \
  }                                                                       \
  /** sus::concepts::SubAssign<##T##> trait. */                           \
  constexpr inline void operator-=(T r)& noexcept {                       \
    auto out =                                                            \
        __private::sub_with_overflow(primitive_value, r.primitive_value); \
    /* TODO: Allow opting out of all overflow checks? */                  \
    ::sus::check(!out.overflow);                                          \
    primitive_value = out.value;                                          \
  }                                                                       \
  /** sus::concepts::MulAssign<##T##> trait. */                           \
  constexpr inline void operator*=(T r)& noexcept {                       \
    auto out =                                                            \
        __private::mul_with_overflow(primitive_value, r.primitive_value); \
    /* TODO: Allow opting out of all overflow checks? */                  \
    ::sus::check(!out.overflow);                                          \
    primitive_value = out.value;                                          \
  }                                                                       \
  /** sus::concepts::DivAssign<##T##> trait. */                           \
  constexpr inline void operator/=(T r)& noexcept {                       \
    /* TODO: Allow opting out of all overflow checks? */                  \
    ::sus::check(r.primitive_value != primitive_type{0});                 \
    /* TODO: Allow opting out of all overflow checks? */                  \
    ::sus::check(primitive_value != MIN_PRIMITIVE ||                      \
                 r.primitive_value != primitive_type{-1});                \
    primitive_value /= r.primitive_value;                                 \
  }                                                                       \
  /** sus::concepts::RemAssign<##T##> trait. */                           \
  constexpr inline void operator%=(T r)& noexcept {                       \
    /* TODO: Allow opting out of all overflow checks? */                  \
    ::sus::check(r.primitive_value != primitive_type{0});                 \
    /* TODO: Allow opting out of all overflow checks? */                  \
    ::sus::check(primitive_value != MIN_PRIMITIVE ||                      \
                 r.primitive_value != primitive_type{-1});                \
    primitive_value %= r.primitive_value;                                 \
  }                                                                       \
  static_assert(true)

#define _sus__signed_mutable_bit_ops(T, UnsignedT)                     \
  /** sus::concepts::BitAndAssign<##T##> trait. */                     \
  constexpr inline void operator&=(T r)& noexcept {                    \
    primitive_value &= r.primitive_value;                              \
  }                                                                    \
  /** sus::concepts::BitOrAssign<##T##> trait. */                      \
  constexpr inline void operator|=(T r)& noexcept {                    \
    primitive_value |= r.primitive_value;                              \
  }                                                                    \
  /** sus::concepts::BitXorAssign<##T##> trait. */                     \
  constexpr inline void operator^=(T r)& noexcept {                    \
    primitive_value ^= r.primitive_value;                              \
  }                                                                    \
  /** sus::concepts::ShlAssign trait. */                               \
  constexpr inline void operator<<=(const u32& r)& noexcept {          \
    /* TODO: Allow opting out of all overflow checks? */               \
    ::sus::check(r < BITS());                                          \
    primitive_value = static_cast<primitive_type>(                     \
        static_cast<UnsignedT>(primitive_value) << r.primitive_value); \
  }                                                                    \
  /** sus::concepts::ShrAssign trait. */                               \
  constexpr inline void operator>>=(const u32& r)& noexcept {          \
    /* TODO: Allow opting out of all overflow checks? */               \
    ::sus::check(r < BITS());                                          \
    primitive_value = static_cast<primitive_type>(                     \
        static_cast<UnsignedT>(primitive_value) >> r.primitive_value); \
  }                                                                    \
  static_assert(true)

#define _sus__signed_abs(T, UnsignedT, UnsignedSusT)                           \
  /** Computes the absolute value of itself.                                   \
   *                                                                           \
   * The absolute value of ##T##::MIN() cannot be represented as an ##T##, and \
   * attempting to calculate it will panic.                                    \
   */                                                                          \
  constexpr inline T abs() const& noexcept {                                   \
    /* TODO: Allow opting out of all overflow checks? */                       \
    ::sus::check(primitive_value != MIN_PRIMITIVE);                            \
    if (primitive_value >= primitive_type{0})                                  \
      return primitive_value;                                                  \
    else                                                                       \
      return -primitive_value;                                                 \
  }                                                                            \
                                                                               \
  /** Checked absolute value. Computes `abs()`, returning None if the current  \
   * value is MIN().                                                           \
   */                                                                          \
  constexpr Option<T> checked_abs() const& noexcept {                          \
    if (primitive_value != MIN_PRIMITIVE) [[likely]]                           \
      return Option<T>::some(abs());                                           \
    else                                                                       \
      return Option<T>::none();                                                \
  }                                                                            \
                                                                               \
  /** Computes the absolute value of self.                                     \
   *                                                                           \
   * Returns a tuple of the absolute version of self along with a boolean      \
   * indicating whether an overflow happened. If self is the minimum value     \
   * (e.g., ##T##::MIN for values of type ##T##), then the minimum value will  \
   * be returned again and true will be returned for an overflow happening.    \
   */                                                                          \
  constexpr Tuple<T, bool> overflowing_abs() const& noexcept {                 \
    if (primitive_value != MIN_PRIMITIVE) [[likely]]                           \
      return Tuple<T, bool>::with(abs(), false);                               \
    else                                                                       \
      return Tuple<T, bool>::with(MIN(), true);                                \
  }                                                                            \
                                                                               \
  /** Saturating absolute value. Computes `abs()`, returning MAX if the        \
   *  current value is MIN() instead of overflowing.                           \
   */                                                                          \
  constexpr T saturating_abs() const& noexcept {                               \
    if (primitive_value != MIN_PRIMITIVE) [[likely]]                           \
      return abs();                                                            \
    else                                                                       \
      return MAX();                                                            \
  }                                                                            \
                                                                               \
  /** Computes the absolute value of self without any wrapping or panicking.   \
   */                                                                          \
  constexpr UnsignedSusT unsigned_abs() const& noexcept {                      \
    if (primitive_value >= primitive_type{0})                                  \
      return static_cast<UnsignedT>(primitive_value);                          \
    else                                                                       \
      return static_cast<UnsignedT>(-(primitive_value + 1)) + UnsignedT{1};    \
  }                                                                            \
                                                                               \
  /** Wrapping (modular) absolute value. Computes `this->abs()`, wrapping      \
   * around at the boundary of the type.                                       \
   *                                                                           \
   * The only case where such wrapping can occur is when one takes the         \
   * absolute value of the negative minimal value for the type; this is a      \
   * positive value that is too large to represent in the type. In such a      \
   * case, this function returns MIN itself.                                   \
   */                                                                          \
  constexpr T wrapping_abs() const& noexcept {                                 \
    if (primitive_value != MIN_PRIMITIVE) [[likely]]                           \
      return abs();                                                            \
    else                                                                       \
      return MIN();                                                            \
  }                                                                            \
                                                                               \
  /** Computes the absolute difference between self and other.                 \
   *                                                                           \
   * This function always returns the correct answer without overflow or       \
   * panics by returning an unsigned integer.                                  \
   */                                                                          \
  constexpr UnsignedSusT abs_diff(const T& r) const& noexcept {                \
    if (primitive_value >= r.primitive_value)                                  \
      return static_cast<UnsignedT>(primitive_value - r.primitive_value);      \
    else                                                                       \
      return static_cast<UnsignedT>(r.primitive_value - primitive_value);      \
  }                                                                            \
  static_assert(true)

#define _sus__signed_add(T, UnsignedT)                                         \
  /** Checked integer addition. Computes self + rhs, returning None if         \
   * overflow occurred.                                                        \
   */                                                                          \
  constexpr Option<T> checked_add(const T& rhs) const& noexcept {              \
    const auto out =                                                           \
        __private::add_with_overflow(primitive_value, rhs.primitive_value);    \
    if (!out.overflow) [[likely]]                                              \
      return Option<T>::some(out.value);                                       \
    else                                                                       \
      return Option<T>::none();                                                \
  }                                                                            \
                                                                               \
  /** Checked integer addition with an unsigned rhs. Computes self + rhs,      \
   * returning None if overflow occurred.                                      \
   */                                                                          \
  constexpr Option<T> checked_add_unsigned(const UnsignedT& rhs)               \
      const& noexcept {                                                        \
    const auto out =                                                           \
        __private::add_with_overflow_unsigned(primitive_value, rhs);           \
    if (!out.overflow) [[likely]]                                              \
      return Option<T>::some(out.value);                                       \
    else                                                                       \
      return Option<T>::none();                                                \
  }                                                                            \
                                                                               \
  /** Calculates self + rhs                                                    \
   *                                                                           \
   * Returns a tuple of the addition along with a boolean indicating whether   \
   * an arithmetic overflow would occur. If an overflow would have occurred    \
   * then the wrapped value is returned.                                       \
   */                                                                          \
  constexpr Tuple<T, bool> overflowing_add(const T& rhs) const& noexcept {     \
    const auto r =                                                             \
        __private::add_with_overflow(primitive_value, rhs.primitive_value);    \
    return Tuple<T, bool>::with(r.value, r.overflow);                          \
  }                                                                            \
                                                                               \
  /** Calculates self + rhs with an unsigned rhs                               \
   *                                                                           \
   * Returns a tuple of the addition along with a boolean indicating whether   \
   * an arithmetic overflow would occur. If an overflow would have occurred    \
   * then the wrapped value is returned.                                       \
   */                                                                          \
  constexpr Tuple<T, bool> overflowing_add_unsigned(const UnsignedT& rhs)      \
      const& noexcept {                                                        \
    const auto r =                                                             \
        __private::add_with_overflow_unsigned(primitive_value, rhs);           \
    return Tuple<T, bool>::with(r.value, r.overflow);                          \
  }                                                                            \
                                                                               \
  /** Saturating integer addition. Computes self + rhs, saturating at the      \
   * numeric bounds instead of overflowing.                                    \
   */                                                                          \
  constexpr T saturating_add(const T& rhs) const& noexcept {                   \
    return __private::saturating_add(primitive_value, rhs.primitive_value);    \
  }                                                                            \
                                                                               \
  /** Saturating integer addition with an unsigned rhs. Computes self + rhs,   \
   * saturating at the numeric bounds instead of overflowing.                  \
   */                                                                          \
  constexpr T saturating_add_unsigned(const UnsignedT& rhs) const& noexcept {  \
    const auto r =                                                             \
        __private::add_with_overflow_unsigned(primitive_value, rhs);           \
    if (!r.overflow) [[likely]]                                                \
      return r.value;                                                          \
    else                                                                       \
      return MAX();                                                            \
  }                                                                            \
                                                                               \
  /** Unchecked integer addition. Computes self + rhs, assuming overflow       \
   * cannot occur.                                                             \
   *                                                                           \
   * # Safety                                                                  \
   * This results in undefined behavior when self + rhs > ##T##::MAX() or self \
   * + rhs < ##T##::MIN(), i.e. when checked_add() would return None.          \
   */                                                                          \
  inline constexpr T unchecked_add(::sus::marker::UnsafeFnMarker,              \
                                   const T& rhs) const& noexcept {             \
    return primitive_value + rhs.primitive_value;                              \
  }                                                                            \
                                                                               \
  /** Wrapping (modular) addition. Computes self + rhs, wrapping around at the \
   * boundary of the type.                                                     \
   */                                                                          \
  constexpr T wrapping_add(const T& rhs) const& noexcept {                     \
    return __private::wrapping_add(primitive_value, rhs.primitive_value);      \
  }                                                                            \
                                                                               \
  /** Wrapping (modular) addition with an unsigned rhs. Computes self + rhs,   \
   * wrapping around at the boundary of the type.                              \
   */                                                                          \
  constexpr T wrapping_add_unsigned(const UnsignedT& rhs) const& noexcept {    \
    return __private::add_with_overflow_unsigned(primitive_value, rhs).value;  \
  }                                                                            \
  static_assert(true)

#define _sus__signed_div(T)                                                    \
  /** Checked integer division. Computes self / rhs, returning None if rhs ==  \
   * 0 or the division results in overflow.                                    \
   */                                                                          \
  constexpr Option<T> checked_div(const T& rhs) const& noexcept {              \
    if (__private::div_overflows(primitive_value, rhs.primitive_value))        \
        [[unlikely]]                                                           \
      return Option<T>::none();                                                \
    else                                                                       \
      return Option<T>::some(primitive_value / rhs.primitive_value);           \
  }                                                                            \
                                                                               \
  /** Calculates the divisor when self is divided by rhs.                      \
   *                                                                           \
   * Returns a tuple of the divisor along with a boolean indicating whether an \
   * arithmetic overflow would occur. If an overflow would occur then self is  \
   * returned.                                                                 \
   *                                                                           \
   * #Panics                                                                   \
   * This function will panic if rhs is 0.                                     \
   */                                                                          \
  constexpr Tuple<T, bool> overflowing_div(const T& rhs) const& noexcept {     \
    /* TODO: Allow opting out of all overflow checks? */                       \
    ::sus::check(rhs != primitive_type{0});                                    \
    if (__private::div_overflows_nonzero(unsafe_fn, primitive_value,           \
                                         rhs.primitive_value)) [[unlikely]] {  \
      return Tuple<T, bool>::with(MIN(), true);                                \
    } else {                                                                   \
      return Tuple<T, bool>::with(primitive_value / rhs.primitive_value,       \
                                  false);                                      \
    }                                                                          \
  }                                                                            \
                                                                               \
  /** Saturating integer division. Computes self / rhs, saturating at the      \
   * numeric bounds instead of overflowing.                                    \
   *                                                                           \
   * #Panics                                                                   \
   * This function will panic if rhs is 0.                                     \
   */                                                                          \
  constexpr T saturating_div(const T& rhs) const& noexcept {                   \
    /* TODO: Allow opting out of all overflow checks? */                       \
    ::sus::check(rhs != primitive_type{0});                                    \
    if (__private::div_overflows_nonzero(unsafe_fn, primitive_value,           \
                                         rhs.primitive_value)) [[unlikely]] {  \
      /* Only overflows in the case of -MIN() / -1, which gives MAX() + 1,     \
       saturated to MAX(). */                                                  \
      return MAX();                                                            \
    } else {                                                                   \
      return primitive_value / rhs.primitive_value;                            \
    }                                                                          \
  }                                                                            \
                                                                               \
  /** Wrapping (modular) division. Computes self / rhs, wrapping around at the \
   * boundary of the type.                                                     \
   *                                                                           \
   * The only case where such wrapping can occur is when one divides MIN / -1  \
   * on a signed type (where MIN is the negative minimal value for the type);  \
   * this is equivalent to -MIN, a positive value that is too large to         \
   * represent in the type. In such a case, this function returns MIN itself.  \
   *                                                                           \
   * #Panics                                                                   \
   * This function will panic if rhs is 0.                                     \
   */                                                                          \
  constexpr T wrapping_div(const T& rhs) const& noexcept {                     \
    /* TODO: Allow opting out of all overflow checks? */                       \
    ::sus::check(rhs != primitive_type{0});                                    \
    if (__private::div_overflows_nonzero(unsafe_fn, primitive_value,           \
                                         rhs.primitive_value)) [[unlikely]] {  \
      /* Only overflows in the case of -MIN() / -1, which gives MAX() + 1,     \
       that wraps around to MIN(). */                                          \
      return MIN();                                                            \
    } else {                                                                   \
      return primitive_value / rhs.primitive_value;                            \
    }                                                                          \
  }                                                                            \
  static_assert(true)

#define _sus__signed_mul(T, LargerT)                                           \
  /** Checked integer multiplication. Computes self * rhs, returning None if   \
   * overflow occurred.                                                        \
   */                                                                          \
  constexpr Option<T> checked_mul(const T& rhs) const& noexcept {              \
    auto out =                                                                 \
        __private::mul_with_overflow(primitive_value, rhs.primitive_value);    \
    if (!out.overflow) [[likely]]                                              \
      return Option<T>::some(out.value);                                       \
    else                                                                       \
      return Option<T>::none();                                                \
  }                                                                            \
                                                                               \
  /** Calculates the multiplication of self and rhs.                           \
   *                                                                           \
   * Returns a tuple of the multiplication along with a boolean indicating     \
   * whether an arithmetic overflow would occur. If an overflow would have     \
   * occurred then the wrapped value is returned.                              \
   */                                                                          \
  constexpr Tuple<T, bool> overflowing_mul(const T& rhs) const& noexcept {     \
    auto r =                                                                   \
        __private::mul_with_overflow(primitive_value, rhs.primitive_value);    \
    return Tuple<T, bool>::with(r.value, r.overflow);                          \
  }                                                                            \
                                                                               \
  /** Saturating integer multiplication. Computes self * rhs, saturating at    \
   * the numeric bounds instead of overflowing.                                \
   */                                                                          \
  constexpr T saturating_mul(const T& rhs) const& noexcept {                   \
    return __private::saturating_mul(primitive_value, rhs.primitive_value);    \
  }                                                                            \
                                                                               \
  /** Unchecked integer multiplication. Computes self * rhs, assuming overflow \
   * cannot occur.                                                             \
   *                                                                           \
   * # Safety                                                                  \
   * This results in undefined behavior when `self * rhs > ##T##::MAX()` or    \
   * `self                                                                     \
   * * rhs < ##T##::MIN()`, i.e. when `checked_mul()` would return None.       \
   */                                                                          \
  constexpr inline T unchecked_mul(::sus::marker::UnsafeFnMarker,              \
                                   const T& rhs) const& noexcept {             \
    return primitive_value * rhs.primitive_value;                              \
  }                                                                            \
                                                                               \
  /** Wrapping (modular) multiplication. Computes self * rhs, wrapping around  \
   * at the boundary of the type.                                              \
   */                                                                          \
  constexpr T wrapping_mul(const T& rhs) const& noexcept {                     \
    return __private::wrapping_mul(primitive_value, rhs.primitive_value);      \
  }                                                                            \
  static_assert(true)

#define _sus__signed_neg(T)                                                   \
  /** Checked negation. Computes -self, returning None if self == MIN.        \
   */                                                                         \
  constexpr Option<T> checked_neg() const& noexcept {                         \
    if (primitive_value != MIN_PRIMITIVE) [[likely]]                          \
      return Option<T>::some(T(-primitive_value));                            \
    else                                                                      \
      return Option<T>::none();                                               \
  }                                                                           \
                                                                              \
  /** Negates self, overflowing if this is equal to the minimum value.        \
   *                                                                          \
   * Returns a tuple of the negated version of self along with a boolean      \
   * indicating whether an overflow happened. If self is the minimum value    \
   * (e.g., ##T##::MIN for values of type ##T##), then the minimum value will \
   * be returned again and true will be returned for an overflow happening.   \
   */                                                                         \
  constexpr Tuple<T, bool> overflowing_neg() const& noexcept {                \
    if (primitive_value != MIN_PRIMITIVE) [[likely]]                          \
      return Tuple<T, bool>::with(-primitive_value, false);                   \
    else                                                                      \
      return Tuple<T, bool>::with(MIN(), true);                               \
  }                                                                           \
                                                                              \
  /** Saturating integer negation. Computes -self, returning MAX if self ==   \
   * MIN instead of overflowing.                                              \
   */                                                                         \
  constexpr T saturating_neg() const& noexcept {                              \
    if (primitive_value != MIN_PRIMITIVE) [[likely]]                          \
      return T(-primitive_value);                                             \
    else                                                                      \
      return MAX();                                                           \
  }                                                                           \
                                                                              \
  /** Wrapping (modular) negation. Computes -self, wrapping around at the     \
   * boundary of the type.                                                    \
   *                                                                          \
   * The only case where such wrapping can occur is when one negates MIN() on \
   * a signed type (where MIN() is the negative minimal value for the type);  \
   * this is a positive value that is too large to represent in the type. In  \
   * such a case, this function returns MIN() itself.                         \
   */                                                                         \
  constexpr T wrapping_neg() const& noexcept {                                \
    if (primitive_value != MIN_PRIMITIVE) [[likely]]                          \
      return -primitive_value;                                                \
    else                                                                      \
      return MIN();                                                           \
  }                                                                           \
  static_assert(true)

#define _sus__signed_rem(T)                                                    \
  /** Checked integer remainder. Computes self % rhs, returning None if rhs == \
   * 0 or the division results in overflow.                                    \
   */                                                                          \
  constexpr Option<T> checked_rem(const T& rhs) const& noexcept {              \
    if (__private::div_overflows(primitive_value, rhs.primitive_value))        \
        [[unlikely]]                                                           \
      return Option<T>::none();                                                \
    else                                                                       \
      return Option<T>::some(primitive_value % rhs.primitive_value);           \
  }                                                                            \
                                                                               \
  /** Calculates the remainder when self is divided by rhs.                    \
   *                                                                           \
   * Returns a tuple of the remainder after dividing along with a boolean      \
   * indicating whether an arithmetic overflow would occur. If an overflow     \
   * would occur then 0 is returned.                                           \
   *                                                                           \
   * # Panics                                                                  \
   * This function will panic if rhs is 0.                                     \
   */                                                                          \
  constexpr Tuple<T, bool> overflowing_rem(const T& rhs) const& noexcept {     \
    /* TODO: Allow opting out of all overflow checks? */                       \
    ::sus::check(rhs != primitive_type{0});                                    \
    if (__private::div_overflows_nonzero(unsafe_fn, primitive_value,           \
                                         rhs.primitive_value)) [[unlikely]] {  \
      return Tuple<T, bool>::with(0, true);                                    \
    } else {                                                                   \
      return Tuple<T, bool>::with(primitive_value % rhs.primitive_value,       \
                                  false);                                      \
    }                                                                          \
  }                                                                            \
                                                                               \
  /** Wrapping (modular) remainder. Computes self % rhs, wrapping around at    \
   * the boundary of the type.                                                 \
   *                                                                           \
   * Such wrap-around never actually occurs mathematically; implementation     \
   * artifacts make x % y invalid for MIN() / -1 on a signed type (where MIN() \
   * is the negative minimal value). In such a case, this function returns 0.  \
   */                                                                          \
  constexpr T wrapping_rem(const T& rhs) const& noexcept {                     \
    /* TODO: Allow opting out of all overflow checks? */                       \
    ::sus::check(rhs != primitive_type{0});                                    \
    if (__private::div_overflows_nonzero(unsafe_fn, primitive_value,           \
                                         rhs.primitive_value)) [[likely]] {    \
      return primitive_type{0};                                                \
    } else {                                                                   \
      return primitive_value % rhs.primitive_value;                            \
    }                                                                          \
  }                                                                            \
  static_assert(true)

#define _sus__signed_euclid(T)                                                 \
  /** Calculates the quotient of Euclidean division of self by rhs.            \
   *                                                                           \
   * This computes the integer q such that self = q * rhs + r, with r =        \
   * self.rem_euclid(rhs) and 0 <= r < abs(rhs).                               \
   *                                                                           \
   * In other words, the result is self / rhs rounded to the integer q such    \
   * that self >= q * rhs. If self > 0, this is equal to round towards zero    \
   * (the default in Rust); if self < 0, this is equal to round towards +/-    \
   * infinity.                                                                 \
   *                                                                           \
   * # Panics                                                                  \
   * This function will panic if rhs is 0 or the division results in overflow. \
   */                                                                          \
  constexpr T div_euclid(const T& rhs) const& noexcept {                       \
    /* TODO: Allow opting out of all overflow checks? */                       \
    ::sus::check(                                                              \
        !__private::div_overflows(primitive_value, rhs.primitive_value));      \
    return __private::div_euclid(unsafe_fn, primitive_value,                   \
                                 rhs.primitive_value);                         \
  }                                                                            \
                                                                               \
  /** Checked Euclidean division. Computes self.div_euclid(rhs), returning     \
   * None if rhs == 0 or the division results in overflow.                     \
   */                                                                          \
  constexpr Option<T> checked_div_euclid(const T& rhs) const& noexcept {       \
    if (__private::div_overflows(primitive_value, rhs.primitive_value))        \
        [[unlikely]] {                                                         \
      return Option<T>::none();                                                \
    } else {                                                                   \
      return Option<T>::some(__private::div_euclid(unsafe_fn, primitive_value, \
                                                   rhs.primitive_value));      \
    }                                                                          \
  }                                                                            \
                                                                               \
  /** Calculates the quotient of Euclidean division self.div_euclid(rhs).      \
   *                                                                           \
   * Returns a tuple of the divisor along with a boolean indicating whether an \
   * arithmetic overflow would occur. If an overflow would occur then self is  \
   * returned.                                                                 \
   *                                                                           \
   * # Panics                                                                  \
   * This function will panic if rhs is 0.                                     \
   */                                                                          \
  constexpr Tuple<T, bool> overflowing_div_euclid(const T& rhs)                \
      const& noexcept {                                                        \
    /* TODO: Allow opting out of all overflow checks? */                       \
    ::sus::check(rhs != primitive_type{0});                                    \
    if (__private::div_overflows_nonzero(unsafe_fn, primitive_value,           \
                                         rhs.primitive_value)) [[unlikely]] {  \
      return Tuple<T, bool>::with(MIN(), true);                                \
    } else {                                                                   \
      return Tuple<T, bool>::with(                                             \
          __private::div_euclid(unsafe_fn, primitive_value,                    \
                                rhs.primitive_value),                          \
          false);                                                              \
    }                                                                          \
  }                                                                            \
                                                                               \
  /** Wrapping Euclidean division. Computes self.div_euclid(rhs), wrapping     \
   * around at the boundary of the type.                                       \
   *                                                                           \
   * Wrapping will only occur in MIN / -1 on a signed type (where MIN is the   \
   * negative minimal value for the type). This is equivalent to -MIN, a       \
   * positive value that is too large to represent in the type. In this case,  \
   * this method returns MIN itself.                                           \
   *                                                                           \
   * # Panics                                                                  \
   * This function will panic if rhs is 0.                                     \
   */                                                                          \
  constexpr T wrapping_div_euclid(const T& rhs) const& noexcept {              \
    /* TODO: Allow opting out of all overflow checks? */                       \
    ::sus::check(rhs != primitive_type{0});                                    \
    if (__private::div_overflows_nonzero(unsafe_fn, primitive_value,           \
                                         rhs.primitive_value)) [[unlikely]] {  \
      return MIN();                                                            \
    } else {                                                                   \
      return __private::div_euclid(unsafe_fn, primitive_value,                 \
                                   rhs.primitive_value);                       \
    }                                                                          \
  }                                                                            \
                                                                               \
  /** Calculates the least nonnegative remainder of self (mod rhs).            \
   *                                                                           \
   * This is done as if by the Euclidean division algorithm – given r =      \
   * self.rem_euclid(rhs), self = rhs * self.div_euclid(rhs) + r, and 0 <= r < \
   * abs(rhs).                                                                 \
   *                                                                           \
   * # Panics                                                                  \
   * This function will panic if rhs is 0 or the division results in overflow. \
   */                                                                          \
  constexpr T rem_euclid(const T& rhs) const& noexcept {                       \
    /* TODO: Allow opting out of all overflow checks? */                       \
    ::sus::check(                                                              \
        !__private::div_overflows(primitive_value, rhs.primitive_value));      \
    return __private::rem_euclid(unsafe_fn, primitive_value,                   \
                                 rhs.primitive_value);                         \
  }                                                                            \
                                                                               \
  /** Checked Euclidean remainder. Computes self.rem_euclid(rhs), returning    \
   * None if rhs == 0 or the division results in overflow.                     \
   */                                                                          \
  constexpr Option<T> checked_rem_euclid(const T& rhs) const& noexcept {       \
    if (__private::div_overflows(primitive_value, rhs.primitive_value))        \
        [[unlikely]] {                                                         \
      return Option<T>::none();                                                \
    } else {                                                                   \
      return Option<T>::some(__private::rem_euclid(unsafe_fn, primitive_value, \
                                                   rhs.primitive_value));      \
    }                                                                          \
  }                                                                            \
                                                                               \
  /** Overflowing Euclidean remainder. Calculates self.rem_euclid(rhs).        \
   *                                                                           \
   * Returns a tuple of the remainder after dividing along with a boolean      \
   * indicating whether an arithmetic overflow would occur. If an overflow     \
   * would occur then 0 is returned.                                           \
   *                                                                           \
   * # Panics                                                                  \
   * This function will panic if rhs is 0.                                     \
   */                                                                          \
  constexpr Tuple<T, bool> overflowing_rem_euclid(const T& rhs)            \
      const& noexcept {                                                        \
    /* TODO: Allow opting out of all overflow checks? */                       \
    ::sus::check(rhs != primitive_type{0});                                    \
    if (__private::div_overflows_nonzero(unsafe_fn, primitive_value,           \
                                         rhs.primitive_value)) [[unlikely]] {  \
      return Tuple<T, bool>::with(0, true);                                    \
    } else {                                                                   \
      return Tuple<T, bool>::with(                                             \
          __private::rem_euclid(unsafe_fn, primitive_value,                    \
                                rhs.primitive_value),                          \
          false);                                                              \
    }                                                                          \
  }                                                                            \
                                                                               \
  /** Wrapping Euclidean remainder. Computes self.rem_euclid(rhs), wrapping    \
   * around at the boundary of the type.                                       \
   *                                                                           \
   * Wrapping will only occur in MIN % -1 on a signed type (where MIN is the   \
   * negative minimal value for the type). In this case, this method returns   \
   * 0.                                                                        \
   *                                                                           \
   * # Panics                                                                  \
   * This function will panic if rhs is 0.                                     \
   */                                                                          \
  constexpr T wrapping_rem_euclid(const T& rhs) const& noexcept {              \
    /* TODO: Allow opting out of all overflow checks? */                       \
    ::sus::check(rhs != primitive_type{0});                                    \
    if (__private::div_overflows_nonzero(unsafe_fn, primitive_value,           \
                                         rhs.primitive_value)) [[unlikely]] {  \
      return primitive_type{0};                                                \
    } else {                                                                   \
      return __private::rem_euclid(unsafe_fn, primitive_value,                 \
                                   rhs.primitive_value);                       \
    }                                                                          \
  }                                                                            \
  static_assert(true)

#define _sus__signed_shift(T)                                                  \
  /** Checked shift left. Computes `*this << rhs`, returning None if rhs is    \
   * larger than or equal to the number of bits in self.                       \
   */                                                                          \
  constexpr Option<T> checked_shl(const u32& rhs) const& noexcept {            \
    auto out =                                                                 \
        __private::shl_with_overflow(primitive_value, rhs.primitive_value);    \
    if (!out.overflow) [[likely]]                                              \
      return Option<T>::some(out.value);                                       \
    else                                                                       \
      return Option<T>::none();                                                \
  }                                                                            \
                                                                               \
  /** Shifts self left by rhs bits.                                            \
   *                                                                           \
   * Returns a tuple of the shifted version of self along with a boolean       \
   * indicating whether the shift value was larger than or equal to the number \
   * of bits. If the shift value is too large, then value is masked (N-1)      \
   * where N is the number of bits, and this value is then used to perform the \
   * shift.                                                                    \
   */                                                                          \
  constexpr Tuple<T, bool> overflowing_shl(const u32& rhs) const& noexcept {   \
    auto r =                                                                   \
        __private::shl_with_overflow(primitive_value, rhs.primitive_value);    \
    return Tuple<T, bool>::with(r.value, r.overflow);                          \
  }                                                                            \
                                                                               \
  /** Panic-free bitwise shift-left; yields `*this << mask(rhs)`, where mask   \
   * removes any high-order bits of `rhs` that would cause the shift to exceed \
   * the bitwidth of the type.                                                 \
   *                                                                           \
   * Note that this is not the same as a rotate-left; the RHS of a wrapping    \
   * shift-left is restricted to the range of the type, rather than the bits   \
   * shifted out of the LHS being returned to the other end. The primitive     \
   * integer types all implement a rotate_left function, which may be what you \
   * want instead.                                                             \
   */                                                                          \
  constexpr T wrapping_shl(const u32& rhs) const& noexcept {                   \
    return __private::shl_with_overflow(primitive_value, rhs.primitive_value)  \
        .value;                                                                \
  }                                                                            \
                                                                               \
  /** Checked shift right. Computes `*this >> rhs`, returning None if rhs is   \
   * larger than or equal to the number of bits in self.                       \
   */                                                                          \
  constexpr Option<T> checked_shr(const u32& rhs) const& noexcept {            \
    auto out =                                                                 \
        __private::shr_with_overflow(primitive_value, rhs.primitive_value);    \
    if (!out.overflow) [[likely]]                                              \
      return Option<T>::some(out.value);                                       \
    else                                                                       \
      return Option<T>::none();                                                \
  }                                                                            \
                                                                               \
  /** Shifts self right by rhs bits.                                           \
   *                                                                           \
   * Returns a tuple of the shifted version of self along with a boolean       \
   * indicating whether the shift value was larger than or equal to the number \
   * of bits. If the shift value is too large, then value is masked (N-1)      \
   * where N is the number of bits, and this value is then used to perform the \
   * shift.                                                                    \
   */                                                                          \
  constexpr Tuple<T, bool> overflowing_shr(const u32& rhs) const& noexcept {   \
    auto r =                                                                   \
        __private::shr_with_overflow(primitive_value, rhs.primitive_value);    \
    return Tuple<T, bool>::with(r.value, r.overflow);                          \
  }                                                                            \
                                                                               \
  /** Panic-free bitwise shift-right; yields `*this >> mask(rhs)`, where mask  \
   * removes any high-order bits of `rhs` that would cause the shift to exceed \
   * the bitwidth of the type.                                                 \
   *                                                                           \
   * Note that this is not the same as a rotate-right; the RHS of a wrapping   \
   * shift-right is restricted to the range of the type, rather than the bits  \
   * shifted out of the LHS being returned to the other end. The primitive     \
   * integer types all implement a rotate_right function, which may be what    \
   * you want instead.                                                         \
   */                                                                          \
  constexpr T wrapping_shr(const u32& rhs) const& noexcept {                   \
    return __private::shr_with_overflow(primitive_value, rhs.primitive_value)  \
        .value;                                                                \
  }                                                                            \
  static_assert(true)

#define _sus__signed_sub(T, UnsignedT)                                        \
  /** Checked integer subtraction. Computes self - rhs, returning None if     \
   * overflow occurred.                                                       \
   */                                                                         \
  constexpr Option<T> checked_sub(const T& rhs) const& {                      \
    auto out =                                                                \
        __private::sub_with_overflow(primitive_value, rhs.primitive_value);   \
    if (!out.overflow) [[likely]]                                             \
      return Option<T>::some(out.value);                                      \
    else                                                                      \
      return Option<T>::none();                                               \
  }                                                                           \
                                                                              \
  /** Checked integer subtraction with an unsigned rhs. Computes self - rhs,  \
   * returning None if overflow occurred.                                     \
   */                                                                         \
  constexpr Option<T> checked_sub_unsigned(const UnsignedT& rhs) const& {     \
    auto out = __private::sub_with_overflow_unsigned(primitive_value, rhs);   \
    if (!out.overflow) [[likely]]                                             \
      return Option<T>::some(out.value);                                      \
    else                                                                      \
      return Option<T>::none();                                               \
  }                                                                           \
                                                                              \
  /** Calculates self - rhs                                                   \
   *                                                                          \
   * Returns a tuple of the subtraction along with a boolean indicating       \
   * whether an arithmetic overflow would occur. If an overflow would have    \
   * occurred then the wrapped value is returned.                             \
   */                                                                         \
  constexpr Tuple<T, bool> overflowing_sub(const T& rhs) const& noexcept {    \
    auto r =                                                                  \
        __private::sub_with_overflow(primitive_value, rhs.primitive_value);   \
    return Tuple<T, bool>::with(r.value, r.overflow);                         \
  }                                                                           \
                                                                              \
  /** Calculates self - rhs with an unsigned rhs.                             \
   *                                                                          \
   * Returns a tuple of the subtraction along with a boolean indicating       \
   * whether an arithmetic overflow would occur. If an overflow would have    \
   * occurred then the wrapped value is returned.                             \
   */                                                                         \
  constexpr Tuple<T, bool> overflowing_sub_unsigned(const UnsignedT& rhs)     \
      const& noexcept {                                                       \
    auto r = __private::sub_with_overflow_unsigned(primitive_value, rhs);     \
    return Tuple<T, bool>::with(r.value, r.overflow);                         \
  }                                                                           \
                                                                              \
  /** Saturating integer subtraction. Computes self - rhs, saturating at the  \
   * numeric bounds instead of overflowing.                                   \
   */                                                                         \
  constexpr T saturating_sub(const T& rhs) const& {                           \
    return __private::saturating_sub(primitive_value, rhs.primitive_value);   \
  }                                                                           \
                                                                              \
  /** Saturating integer subtraction with an unsigned rhs. Computes self -    \
   * rhs, saturating at the numeric bounds instead of overflowing.            \
   */                                                                         \
  constexpr T saturating_sub_unsigned(const UnsignedT& rhs) const& {          \
    auto r = __private::sub_with_overflow_unsigned(primitive_value, rhs);     \
    if (!r.overflow) [[likely]]                                               \
      return r.value;                                                         \
    else                                                                      \
      return MIN();                                                           \
  }                                                                           \
                                                                              \
  /** Unchecked integer subtraction. Computes self - rhs, assuming overflow   \
   * cannot occur.                                                            \
   */                                                                         \
  constexpr T unchecked_sub(::sus::marker::UnsafeFnMarker, const T& rhs)      \
      const& {                                                                \
    return primitive_value - rhs.primitive_value;                             \
  }                                                                           \
                                                                              \
  /** Wrapping (modular) subtraction. Computes self - rhs, wrapping around at \
   * the boundary of the type.                                                \
   */                                                                         \
  constexpr T wrapping_sub(const T& rhs) const& {                             \
    return __private::wrapping_sub(primitive_value, rhs.primitive_value);     \
  }                                                                           \
                                                                              \
  /** Wrapping (modular) subtraction with an unsigned rhs. Computes self -    \
   * rhs, wrapping around at the boundary of the type.                        \
   */                                                                         \
  constexpr T wrapping_sub_unsigned(const UnsignedT& rhs) const& {            \
    return __private::sub_with_overflow_unsigned(primitive_value, rhs).value; \
  }                                                                           \
  static_assert(true)

#define _sus__signed_bits(T, UnsignedT)                                        \
  /** Returns the number of ones in the binary representation of the current   \
   * value.                                                                    \
   */                                                                          \
  constexpr u32 count_ones() const& noexcept {                                 \
    return __private::count_ones(static_cast<UnsignedT>(primitive_value));     \
  }                                                                            \
                                                                               \
  /** Returns the number of zeros in the binary representation of the current  \
   * value.                                                                    \
   */                                                                          \
  constexpr u32 count_zeros() const& noexcept {                                \
    return (~(*this)).count_ones();                                            \
  }                                                                            \
                                                                               \
  /** Returns the number of leading ones in the binary representation of the   \
   * current value.                                                            \
   */                                                                          \
  constexpr /* TODO:u32 */ uint32_t leading_ones() const& noexcept {           \
    return (~(*this)).leading_zeros();                                         \
  }                                                                            \
                                                                               \
  /** Returns the number of leading zeros in the binary representation of the  \
   * current value.                                                            \
   */                                                                          \
  constexpr /* TODO:u32 */ uint32_t leading_zeros() const& noexcept {          \
    return __private::leading_zeros(static_cast<UnsignedT>(primitive_value));  \
  }                                                                            \
                                                                               \
  /** Returns the number of trailing ones in the binary representation of the  \
   * current value.                                                            \
   */                                                                          \
  constexpr /* TODO:u32 */ uint32_t trailing_ones() const& noexcept {          \
    return (~(*this)).trailing_zeros();                                        \
  }                                                                            \
                                                                               \
  /** Returns the number of trailing zeros in the binary representation of the \
   * current value.                                                            \
   */                                                                          \
  constexpr /* TODO:u32 */ uint32_t trailing_zeros() const& noexcept {         \
    return __private::trailing_zeros(static_cast<UnsignedT>(primitive_value)); \
  }                                                                            \
                                                                               \
  /** Reverses the order of bits in the integer. The least significant bit     \
   * becomes the most significant bit, second least-significant bit becomes    \
   * second most-significant bit, etc.                                         \
   */                                                                          \
  constexpr T reverse_bits() const& noexcept {                                 \
    return static_cast<primitive_type>(                                        \
        __private::reverse_bits(static_cast<UnsignedT>(primitive_value)));     \
  }                                                                            \
                                                                               \
  /** Shifts the bits to the left by a specified amount, `n`, wrapping the     \
   * truncated bits to the end of the resulting integer.                       \
   *                                                                           \
   * Please note this isn't the same operation as the `<<` shifting operator!  \
   */                                                                          \
  constexpr T rotate_left(const u32& n) const& noexcept {                      \
    return static_cast<primitive_type>(__private::rotate_left(                 \
        static_cast<UnsignedT>(primitive_value), n.primitive_value));          \
  }                                                                            \
                                                                               \
  /** Shifts the bits to the right by a specified amount, n, wrapping the      \
   * truncated bits to the beginning of the resulting integer.                 \
   *                                                                           \
   * Please note this isn't the same operation as the >> shifting operator!    \
   */                                                                          \
  constexpr T rotate_right(const u32& n) const& noexcept {                     \
    return static_cast<primitive_type>(__private::rotate_right(                \
        static_cast<UnsignedT>(primitive_value), n.primitive_value));          \
  }                                                                            \
                                                                               \
  /** Reverses the byte order of the integer.                                  \
   */                                                                          \
  constexpr T swap_bytes() const& noexcept {                                   \
    return static_cast<primitive_type>(                                        \
        __private::swap_bytes(static_cast<UnsignedT>(primitive_value)));       \
  }                                                                            \
  static_assert(true)

#define _sus__signed_pow(T)                                                    \
  /**  Raises self to the power of `exp`, using exponentiation by squaring. */ \
  constexpr inline T pow(const u32& rhs) const& noexcept {                     \
    auto out =                                                                 \
        __private::pow_with_overflow(primitive_value, rhs.primitive_value);    \
    /* TODO: Allow opting out of all overflow checks? */                       \
    ::sus::check(!out.overflow);                                               \
    return out.value;                                                          \
  }                                                                            \
                                                                               \
  /** Checked exponentiation. Computes `##T##::pow(exp)`, returning None if    \
   * overflow occurred.                                                        \
   */                                                                          \
  constexpr Option<T> checked_pow(const u32& rhs) const& noexcept {            \
    auto out =                                                                 \
        __private::pow_with_overflow(primitive_value, rhs.primitive_value);    \
    /* TODO: Allow opting out of all overflow checks? */                       \
    if (!out.overflow) [[likely]]                                              \
      return Option<T>::some(out.value);                                       \
    else                                                                       \
      return Option<T>::none();                                                \
  }                                                                            \
                                                                               \
  /** Raises self to the power of `exp`, using exponentiation by squaring.     \
   *                                                                           \
   * Returns a tuple of the exponentiation along with a bool indicating        \
   * whether an overflow happened.                                             \
   */                                                                          \
  constexpr Tuple<T, bool> overflowing_pow(const u32& exp) const& noexcept {   \
    auto r =                                                                   \
        __private::pow_with_overflow(primitive_value, exp.primitive_value);    \
    return Tuple<T, bool>::with(r.value, r.overflow);                          \
  }                                                                            \
                                                                               \
  /** Wrapping (modular) exponentiation. Computes self.pow(exp), wrapping      \
   * around at the boundary of the type.                                       \
   */                                                                          \
  constexpr T wrapping_pow(const u32& exp) const& noexcept {                   \
    return __private::wrapping_pow(primitive_value, exp.primitive_value);      \
  }                                                                            \
  static_assert(true)

#define _sus__signed_log(T, UnsignedT)                                        \
  /** Returns the base 2 logarithm of the number, rounded down.               \
   *                                                                          \
   * Returns None if the number is negative or zero.                          \
   */                                                                         \
  constexpr Option<u32> checked_log2() const& {                               \
    if (primitive_value <= primitive_type{0}) [[unlikely]] {                  \
      return Option<u32>::none();                                             \
    } else {                                                                  \
      uint32_t zeros = __private::leading_zeros_nonzero(                      \
          unsafe_fn, static_cast<UnsignedT>(primitive_value));                \
      return Option<u32>::some(BITS() - 1_u32 - u32(zeros));                  \
    }                                                                         \
  }                                                                           \
                                                                              \
  /** Returns the base 2 logarithm of the number, rounded down.               \
   *                                                                          \
   * # Panics                                                                 \
   * When the number is zero or negative the function will panic. \           \
   *  */                                                                      \
  constexpr u32 log2() const& {                                               \
    /* TODO: Allow opting out of all overflow checks? */                      \
    return checked_log2().unwrap();                                           \
  }                                                                           \
                                                                              \
  /** Returns the base 10 logarithm of the number, rounded down.              \
   *                                                                          \
   * Returns None if the number is negative or zero.                          \
   */                                                                         \
  constexpr Option<u32> checked_log10() const& {                              \
    if (primitive_value <= primitive_type{0}) [[unlikely]] {                  \
      return Option<u32>::none();                                             \
    } else {                                                                  \
      return Option<u32>::some(__private::int_log10::T(primitive_value));     \
    }                                                                         \
  }                                                                           \
                                                                              \
  /** Returns the base 10 logarithm of the number, rounded down.              \
   *                                                                          \
   * # Panics                                                                 \
   * When the number is zero or negative the function will panic.             \
   */                                                                         \
  constexpr u32 log10() const& {                                              \
    /* TODO: Allow opting out of all overflow checks? */                      \
    return checked_log10().unwrap();                                          \
  }                                                                           \
                                                                              \
  /** Returns the logarithm of the number with respect to an arbitrary base,  \
   * rounded down.                                                            \
   *                                                                          \
   * Returns None if the number is negative or zero, or if the base is not at \
   * least 2.                                                                 \
   *                                                                          \
   * This method might not be optimized owing to implementation details;      \
   * `checked_log2` can produce results more efficiently for base 2, and      \
   * `checked_log10` can produce results more efficiently for base 10.        \
   */                                                                         \
  constexpr Option<u32> checked_log(const T& base) const& noexcept {          \
    if (primitive_value <= primitive_type{0} ||                               \
        base.primitive_value <= primitive_type{1}) [[unlikely]] {             \
      return Option<u32>::none();                                             \
    } else {                                                                  \
      auto n = 0_u32;                                                         \
      auto r = primitive_value;                                               \
      const auto b = base.primitive_value;                                    \
      while (r >= b) {                                                        \
        r /= b;                                                               \
        n += 1_u32;                                                           \
      }                                                                       \
      return Option<u32>::some(n);                                            \
    }                                                                         \
  }                                                                           \
                                                                              \
  /** Returns the logarithm of the number with respect to an arbitrary base,  \
   * rounded down.                                                            \
   *                                                                          \
   * This method might not be optimized owing to implementation details; log2 \
   * can produce results more efficiently for base 2, and log10 can produce   \
   * results more efficiently for base 10.                                    \
   *                                                                          \
   * # Panics                                                                 \
   * When the number is negative, zero, or if the base is not at least 2.     \
   */                                                                         \
  constexpr u32 log(const T& base) const& noexcept {                          \
    return checked_log(base).unwrap();                                        \
  }                                                                           \
  static_assert(true)

#define _sus__signed_endian(T, UnsignedT, Bytes)                              \
  /** Converts an integer from big endian to the target's endianness.         \
   *                                                                          \
   * On big endian this is a no-op. On little endian the bytes are swapped.   \
   */                                                                         \
  static constexpr T from_be(const T& x) noexcept {                           \
    if (::sus::assertions::is_big_endian())                                   \
      return x;                                                               \
    else                                                                      \
      return x.swap_bytes();                                                  \
  }                                                                           \
                                                                              \
  /** Converts an integer from little endian to the target's endianness.      \
   *                                                                          \
   * On little endian this is a no-op. On big endian the bytes are swapped.   \
   */                                                                         \
  static constexpr T from_le(const T& x) noexcept {                           \
    if (::sus::assertions::is_little_endian())                                \
      return x;                                                               \
    else                                                                      \
      return x.swap_bytes();                                                  \
  }                                                                           \
                                                                              \
  /** Converts self to big endian from the target's endianness.               \
   *                                                                          \
   * On big endian this is a no-op. On little endian the bytes are swapped.   \
   */                                                                         \
  constexpr T to_be() const& noexcept {                                       \
    if (::sus::assertions::is_big_endian())                                   \
      return *this;                                                           \
    else                                                                      \
      return swap_bytes();                                                    \
  }                                                                           \
                                                                              \
  /** Converts self to little endian from the target's endianness.            \
   *                                                                          \
   * On little endian this is a no-op. On big endian the bytes are swapped.   \
   */                                                                         \
  constexpr T to_le() const& noexcept {                                       \
    if (::sus::assertions::is_little_endian())                                \
      return *this;                                                           \
    else                                                                      \
      return swap_bytes();                                                    \
  }                                                                           \
                                                                              \
  /** Return the memory representation of this integer as a byte array in     \
   * big-endian (network) byte order.                                         \
   */                                                                         \
  constexpr ::sus::Array</* TODO: u8 */ uint8_t, Bytes> to_be_bytes()         \
      const& noexcept {                                                       \
    return to_be().to_ne_bytes();                                             \
  }                                                                           \
                                                                              \
  /** Return the memory representation of this integer as a byte array in     \
   * little-endian byte order.                                                \
   */                                                                         \
  constexpr ::sus::Array</* TODO: u8 */ uint8_t, Bytes> to_le_bytes()         \
      const& noexcept {                                                       \
    return to_le().to_ne_bytes();                                             \
  }                                                                           \
                                                                              \
  /** Return the memory representation of this integer as a byte array in     \
   * native byte order.                                                       \
   *                                                                          \
   * As the target platform's native endianness is used, portable code should \
   * use `to_be_bytes()` or `to_le_bytes()`, as appropriate, instead.         \
   */                                                                         \
  constexpr ::sus::Array</* TODO: u8 */ uint8_t, Bytes> to_ne_bytes()         \
      const& noexcept {                                                       \
    auto bytes =                                                              \
        ::sus::Array</* TODO: u8 */ uint8_t, sizeof(T)>::with_uninitialized(  \
            unsafe_fn);                                                       \
    if (std::is_constant_evaluated()) {                                       \
      auto uval = static_cast<UnsignedT>(primitive_value);                    \
      for (auto i = size_t{0}; i < sizeof(T); ++i) {                          \
        if (sus::assertions::is_little_endian())                              \
          bytes.get_mut(i) = uval & 0xff;                                     \
        else                                                                  \
          bytes.get_mut(sizeof(T) - 1 - i) = uval & 0xff;                     \
        uval >>= 8u;                                                          \
      }                                                                       \
    } else {                                                                  \
      memcpy(bytes.as_ptr_mut(), &primitive_value, sizeof(T));                \
    }                                                                         \
    return bytes;                                                             \
  }                                                                           \
                                                                              \
  /** Create an integer value from its representation as a byte array in big  \
   * endian.                                                                  \
   */                                                                         \
  static constexpr T from_be_bytes(                                           \
      const ::sus::Array</*TODO: u8*/ uint8_t, Bytes>& bytes) noexcept {      \
    return from_be(from_ne_bytes(bytes));                                     \
  }                                                                           \
                                                                              \
  /** Create an integer value from its representation as a byte array in      \
   * little endian.                                                           \
   */                                                                         \
  static constexpr T from_le_bytes(                                           \
      const ::sus::Array</*TODO: u8*/ uint8_t, Bytes>& bytes) noexcept {      \
    return from_le(from_ne_bytes(bytes));                                     \
  }                                                                           \
                                                                              \
  /** Create an integer value from its memory representation as a byte array  \
   * in native endianness.                                                    \
   *                                                                          \
   * As the target platform's native endianness is used, portable code likely \
   * wants to use `from_be_bytes()` or `from_le_bytes()`, as appropriate      \
   * instead.                                                                 \
   */                                                                         \
  static constexpr T from_ne_bytes(                                           \
      const ::sus::Array</*TODO: u8*/ uint8_t, Bytes>& bytes) noexcept {      \
    UnsignedT val;                                                            \
    if (std::is_constant_evaluated()) {                                       \
      val = UnsignedT{0};                                                     \
      for (auto i = size_t{0}; i < sizeof(T); ++i) {                          \
        val |= bytes.get(i) << (sizeof(T) - 1 - i);                           \
      }                                                                       \
    } else {                                                                  \
      memcpy(&val, bytes.as_ptr(), sizeof(T));                                \
    }                                                                         \
    return static_cast<primitive_type>(val);                                  \
  }                                                                           \
  static_assert(true)

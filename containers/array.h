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

#include <concepts>
#include <type_traits>
#include <utility>  // TODO: replace std::make_index_sequence.

#include "assertions/check.h"
#include "construct/make_default.h"
#include "containers/array_iter.h"
#include "containers/slice_iter.h"
#include "fn/callable.h"
#include "fn/fn_defn.h"
#include "marker/unsafe.h"
#include "mem/move.h"
#include "mem/relocate.h"
#include "num/num_concepts.h"
#include "num/unsigned_integer.h"
#include "ops/eq.h"
#include "ops/ord.h"

namespace sus::containers {

namespace __private {

template <class T, size_t N>
struct Storage final {
  T data_[N];
};

template <class T>
struct Storage<T, 0> final {};

}  // namespace __private

/// A container of objects of type T, with a fixed size N.
///
/// An Array can not be larger than PTRDIFF_MAX, as subtracting a pointer at a
/// greater distance results in Undefined Behaviour.
template <class T, size_t N>
  requires(N <= PTRDIFF_MAX)
class Array final {
  static_assert(!std::is_const_v<T>);

 public:
  constexpr static Array with_default() noexcept
    requires(::sus::construct::MakeDefault<T>)
  {
    auto a = Array(kWithUninitialized);
    if constexpr (N > 0) {
      for (size_t i = 0; i < N; ++i)
        a.storage_.data_[i] = ::sus::construct::make_default<T>();
    }
    return a;
  }

  constexpr static Array with_uninitialized(
      ::sus::marker::UnsafeFnMarker) noexcept {
    return Array(kWithUninitialized);
  }

  template <::sus::fn::callable::CallableReturns<T> InitializerFn>
  constexpr static Array with_initializer(InitializerFn f) noexcept {
    return Array(kWithInitializer, move(f), std::make_index_sequence<N>());
  }

  /// Uses convertible_to<T> to accept `sus::into()` values. But doesn't use
  /// sus::construct::Into<T> to avoid implicit conversions.
  template <std::convertible_to<T> U>
  constexpr static Array with_value(const U& t) noexcept {
    return Array(kWithValue, t, std::make_index_sequence<N>());
  }

  /// Uses convertible_to<T> instead of same_as<T> to accept `sus::into()`
  /// values. But doesn't use sus::construct::Into<T> to avoid implicit
  /// conversions.
  template <std::convertible_to<T>... Ts>
    requires(sizeof...(Ts) == N)
  constexpr static Array with_values(Ts... ts) noexcept {
    auto a = Array(kWithUninitialized);
    init_values(a.as_ptr_mut(), 0, move(ts)...);
    return a;
  }

  /// Returns the number of elements in the array.
  constexpr usize len() const& noexcept { return N; }

  // TODO: Add index operators like slice.
  // TODO: Make get/get_mut return Option.

  /// Returns a const reference to the element at index `i`.
  constexpr Option<const T&> get(usize i) const& noexcept
    requires(N > 0)
  {
    if (i.primitive_value >= N) [[unlikely]]
      return Option<const T&>::none();
    return Option<const T&>::some(storage_.data_[i.primitive_value]);
  }
  constexpr Option<const T&> get(usize i) && = delete;

  /// Returns a mutable reference to the element at index `i`.
  constexpr Option<T&> get_mut(usize i) & noexcept
    requires(N > 0)
  {
    if (i.primitive_value >= N) [[unlikely]]
      return Option<T&>::none();
    return Option<T&>::some(mref(storage_.data_[i.primitive_value]));
  }

  /// Returns a const reference to the element at index `i`.
  constexpr const T& get_unchecked(::sus::marker::UnsafeFnMarker,
                                   usize i) const& noexcept
    requires(N > 0)
  {
    return storage_.data_[i.primitive_value];
  }
  constexpr const T& get_unchecked(::sus::marker::UnsafeFnMarker,
                                   usize i) && = delete;

  /// Returns a mutable reference to the element at index `i`.
  constexpr T& get_unchecked_mut(::sus::marker::UnsafeFnMarker,
                                 usize i) & noexcept
    requires(N > 0)
  {
    return storage_.data_[i.primitive_value];
  }

  constexpr const T& operator[](usize i) const& noexcept {
    check(i < usize{N});
    return storage_.data_[i.primitive_value];
  }
  constexpr const T& operator[](usize i) && = delete;

  constexpr T& operator[](usize i) & noexcept {
    check(i < usize{N});
    return storage_.data_[i.primitive_value];
  }

  /// Returns a const pointer to the first element in the array.
  const T* as_ptr() const& noexcept
    requires(N > 0)
  {
    return storage_.data_;
  }
  const T* as_ptr() && = delete;

  /// Returns a mutable pointer to the first element in the array.
  T* as_ptr_mut() & noexcept
    requires(N > 0)
  {
    return storage_.data_;
  }

  /// Returns an iterator over all the elements in the array, visited in the
  /// same order they appear in the array. The iterator gives const access to
  /// each element.
  ::sus::iter::Iterator<SliceIter<T>> iter() const& noexcept {
    return SliceIter<T>::with(storage_.data_, N);
  }
  ::sus::iter::Iterator<SliceIter<T>> iter() && = delete;

  /// Returns an iterator over all the elements in the array, visited in the
  /// same order they appear in the array. The iterator gives mutable access to
  /// each element.
  ::sus::iter::Iterator<SliceIterMut<T>> iter_mut() & noexcept {
    return SliceIterMut<T>::with(storage_.data_, N);
  }

  /// Converts the array into an iterator that consumes the array and returns
  /// each element in the same order they appear in the array.
  ::sus::iter::Iterator<ArrayIntoIter<T, N>> into_iter() && noexcept {
    return ArrayIntoIter<T, N>::with(static_cast<Array&&>(*this));
  }

  /// Consumes the array, and returns a new array, mapping each element of the
  /// array to a new type with the given function.
  ///
  /// To just walk each element and map them, consider using `iter()` and
  /// `Iterator::map`. This does not require consuming the array.
  template <::sus::fn::callable::CallableWith<T&&> MapFn, int&...,
            class R = std::invoke_result_t<MapFn, T&&>>
    requires(N > 0 && !std::is_void_v<R>)
  Array<R, N> map(MapFn f) && noexcept {
    return Array<R, N>::with_initializer([this, &f, i = size_t{0}]() mutable {
      return f(move(storage_.data_[i++]));
    });
  }

  /// sus::ops::Eq<Array<U, N>> trait.
  template <::sus::ops::Eq<T> U>
  constexpr bool operator==(const Array<U, N>& r) const& noexcept {
    return eq_impl(r, std::make_index_sequence<N>());
  }

 private:
  enum WithInitializer { kWithInitializer };
  template <class F, size_t... Is>
  constexpr Array(WithInitializer, F&& f, std::index_sequence<Is...>) noexcept
      : storage_{((void)Is, f())...} {}

  enum WithValue { kWithValue };
  template <size_t... Is>
  constexpr Array(WithValue, const T& t, std::index_sequence<Is...>) noexcept
      : storage_{((void)Is, t)...} {}

  enum WithUninitialized { kWithUninitialized };
  template <size_t... Is>
  constexpr Array(WithUninitialized) noexcept {}

  template <std::convertible_to<T> T1, std::convertible_to<T>... Ts>
  static inline void init_values(T* a, size_t index, T1&& t1, Ts&&... ts) {
    new (a + index) T(move(t1));
    init_values(a, index + 1, move(ts)...);
  }
  template <std::convertible_to<T> T1>
  static inline void init_values(T* a, size_t index, T1&& t1) {
    new (a + index) T(move(t1));
  }

  template <class U, size_t... Is>
  constexpr inline auto eq_impl(const Array<U, N>& r,
                                std::index_sequence<Is...>) const& noexcept {
    return (true && ... && (get(Is) == r.get(Is)));
  };

  // Using a union ensures that the default constructor doesn't initialize
  // anything.
  union {
    ::sus::containers::__private::Storage<T, N> storage_;
  };

  sus_class_trivial_relocatable_value(unsafe_fn,
                                      ::sus::mem::relocate_array_by_memcpy<T>);
};

namespace __private {

template <size_t I, class O, class T, class U, size_t N>
constexpr inline bool array_cmp_impl(O& val, const Array<T, N>& l,
                                     const Array<U, N>& r) noexcept {
  auto cmp = l.get(I) <=> r.get(I);
  // Allow downgrading from equal to equivalent, but not the inverse.
  if (cmp != 0) val = cmp;
  // Short circuit by returning true when we find a difference.
  return val == 0;
};

template <class T, class U, size_t N, size_t... Is>
constexpr inline auto array_cmp(auto equal, const Array<T, N>& l,
                                const Array<U, N>& r,
                                std::index_sequence<Is...>) noexcept {
  auto val = equal;
  (true && ... && (array_cmp_impl<Is>(val, l, r)));
  return val;
};

}  // namespace __private

/// sus::ops::Ord<Option<U>> trait.
template <class T, class U, size_t N>
  requires(::sus::ops::ExclusiveOrd<T, U>)
constexpr inline auto operator<=>(const Array<T, N>& l,
                                  const Array<U, N>& r) noexcept {
  return __private::array_cmp(std::strong_ordering::equivalent, l, r,
                              std::make_index_sequence<N>());
}

/// sus::ops::Ord<Option<U>> trait.
template <class T, class U, size_t N>
  requires(::sus::ops::ExclusiveWeakOrd<T, U>)
constexpr inline auto operator<=>(const Array<T, N>& l,
                                  const Array<U, N>& r) noexcept {
  return __private::array_cmp(std::weak_ordering::equivalent, l, r,
                              std::make_index_sequence<N>());
}

/// sus::ops::Ord<Option<U>> trait.
template <class T, class U, size_t N>
  requires(::sus::ops::ExclusivePartialOrd<T, U>)
constexpr inline auto operator<=>(const Array<T, N>& l,
                                  const Array<U, N>& r) noexcept {
  return __private::array_cmp(std::partial_ordering::equivalent, l, r,
                              std::make_index_sequence<N>());
}

// Implicit for-ranged loop iteration via `Array::iter()`.
using ::sus::iter::__private::begin;
using ::sus::iter::__private::end;

}  // namespace sus::containers

namespace sus {
using containers::Array;
}

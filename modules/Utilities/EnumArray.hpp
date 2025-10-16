//-------------------------------------------------------------------------------------------------
// /Utilities/EnumArray.hpp
// The nModules Project
//
// Lets you easily create and manage an array based on a class enum.
//
// There are three requirements :
// 1. The first element in the enum class must be = 0.
// 2. No other values may be specified.
// 3. The last element of the enum must be Count.
//-------------------------------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdarg>
#include <functional>
#include <initializer_list>
#include <type_traits>

/// <summary>
/// Increments the value of enumVal
/// </summary>
template <typename EnumType>
static EnumType EnumIncrement(EnumType &enumVal) {
  using Underlying = typename std::underlying_type<EnumType>::type;
  return enumVal = static_cast<EnumType>(static_cast<Underlying>(enumVal) + 1);
}

/// <summary>
/// Decrements the value of enumVal
/// </summary>
template <typename EnumType>
static EnumType EnumDecrement(EnumType &enumVal) {
  using Underlying = typename std::underlying_type<EnumType>::type;
  return enumVal = static_cast<EnumType>(static_cast<Underlying>(enumVal) - 1);
}

template<class ElementType, class IndexType>
class EnumArray {
public:
  static_assert(std::is_enum<IndexType>::value, "IndexType must be an enum type");
  static constexpr std::size_t kCount = static_cast<std::size_t>(IndexType::Count);

public:
  /// <summary>
  /// Constructor
  /// </summary>
  EnumArray() = default;

  /// <summary>
  /// Constructor
  /// </summary>
  explicit EnumArray(ElementType start, ...) {
    mArray[0] = start;

    va_list list;
    va_start(list, start);
    for (std::size_t i = 1; i < kCount; ++i) {
      mArray[i] = va_arg(list, ElementType);
    }
    va_end(list);
  }

  /// <summary>
  /// Constructor
  /// </summary>
  explicit EnumArray(std::initializer_list<ElementType> init) {
    ElementType *element = mArray;
    for (ElementType initializer : init) {
      if (element >= mArray + kCount) {
        break;
      }
      *element++ = initializer;
    }
  }

  /// <summary>
  /// Callback constructor.
  /// </summary>
  explicit EnumArray(std::function<void(EnumArray<ElementType, IndexType> &)> init) {
    init(*this);
  }

public:
  /// <summary>
  /// Retrives the element correspoding to the specified index.
  /// </summary>
  ElementType &operator[](IndexType index) {
    return mArray[static_cast<std::size_t>(index)];
  }

  /// <summary>
  /// Retrives the element correspoding to the specified index.
  /// </summary>
  const ElementType &operator[](IndexType index) const {
    return mArray[static_cast<std::size_t>(index)];
  }

  /// <summary>
  /// Returns a pointer to the first element in the array.
  /// </summary>
  ElementType *begin() {
    return mArray;
  }

  const ElementType *begin() const {
    return mArray;
  }

  /// <summary>
  /// Returns a pointer to the one-past-last element in the array.
  /// </summary>
  ElementType *end() {
    return mArray + kCount;
  }

  const ElementType *end() const {
    return mArray + kCount;
  }

  /// <summary>
  /// Sets every element in the array to some value.
  /// </summary>
  void SetAll(ElementType value) {
    for (ElementType &element : mArray) {
      element = value;
    }
  }

private:
  /// <summary>
  /// Contains the actual array elements.
  /// </summary>
  ElementType mArray[kCount > 0 ? kCount : 1];
};

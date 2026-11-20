#ifndef CHECKSUM_PRIVATE_VECTOR_ARRAY_HPP
#define CHECKSUM_PRIVATE_VECTOR_ARRAY_HPP

// std::array for SIMD vectors. The MSVC standard library declares std::array under #pragma pack(8), which clang-cl, unlike
// MSVC, applies to a vector type argument: there alignof(std::array<__m256i, 4>) is 8 and unoptimized code faults on it.

#include <cstddef>

namespace checksum::isa_detail {

template <class Vector, std::size_t Size> struct vector_array {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays,hicpp-avoid-c-arrays): std::array is what this replaces.
  Vector elements[Size];

  constexpr Vector &operator[](std::size_t index) noexcept;
  constexpr Vector const &operator[](std::size_t index) const noexcept;
  static constexpr std::size_t size() noexcept;
};

template <class Vector, std::size_t Size> constexpr Vector &vector_array<Vector, Size>::operator[](std::size_t index) noexcept {
  return elements[index];
}

template <class Vector, std::size_t Size> constexpr Vector const &vector_array<Vector, Size>::operator[](std::size_t index) const noexcept {
  return elements[index];
}

template <class Vector, std::size_t Size> constexpr std::size_t vector_array<Vector, Size>::size() noexcept { return Size; }

} // namespace checksum::isa_detail

#endif // CHECKSUM_PRIVATE_VECTOR_ARRAY_HPP

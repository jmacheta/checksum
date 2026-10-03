#ifndef CHECKSUM_BYTE_RANGE_HPP
#define CHECKSUM_BYTE_RANGE_HPP

#include <array>
#include <concepts>
#include <cstddef>
#include <ranges>
#include <span>
#include <type_traits>

/**
 * @addtogroup checksum
 * @{
 */

namespace checksum {

/// std::byte, char, unsigned char, signed char or char8_t, optionally cv-qualified.
template <class Type>
concept byte_like = std::same_as<std::remove_cv_t<Type>, std::byte> || std::same_as<std::remove_cv_t<Type>, char> ||
                    std::same_as<std::remove_cv_t<Type>, unsigned char> || std::same_as<std::remove_cv_t<Type>, signed char> ||
                    std::same_as<std::remove_cv_t<Type>, char8_t>;

/// An input range of non-volatile byte_like values. Arrays of char and char8_t are rejected so that the '\0' of a string literal is never hashed:
/// pass text as std::string_view.
template <class Range>
concept byte_range = std::ranges::input_range<Range> && byte_like<std::ranges::range_value_t<Range>> &&
                     !std::is_volatile_v<std::remove_reference_t<std::ranges::range_reference_t<Range>>> &&
                     !(std::is_array_v<std::remove_cvref_t<Range>> && (std::same_as<std::remove_cv_t<std::ranges::range_value_t<Range>>, char> ||
                                                                       std::same_as<std::remove_cv_t<std::ranges::range_value_t<Range>>, char8_t>));

} // namespace checksum

/// Implementation details shared by the algorithms; not part of the public API.
namespace checksum::detail {

// Calls visit(std::span<std::byte const>) once for a contiguous range at run time, otherwise once per 64-byte chunk copied to the stack.
template <byte_range Range, class Visitor> constexpr void for_each_chunk(Range &&data, Visitor visit) noexcept;

} // namespace checksum::detail

///@}

namespace checksum::detail {

// data is deliberately used as an lvalue: std::ranges::data rejects non-borrowed rvalue ranges.
// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
template <byte_range Range, class Visitor> constexpr void for_each_chunk(Range &&data, Visitor visit) noexcept {
  using value_type = std::remove_cv_t<std::ranges::range_value_t<Range>>;
  if constexpr(std::ranges::contiguous_range<Range> && std::ranges::sized_range<Range>) {
    if constexpr(std::same_as<value_type, std::byte>) {
      visit(std::span<std::byte const>(std::ranges::data(data), std::ranges::size(data)));
      return;
    } else {
      if !consteval {
        visit(std::as_bytes(std::span(std::ranges::data(data), std::ranges::size(data))));
        return;
      }
    }
  }
  constexpr std::size_t chunk_size = 64;
  std::array<std::byte, chunk_size> chunk{};
  std::size_t used = 0;
  for(auto &&value : data) {
    chunk[used++] = static_cast<std::byte>(static_cast<unsigned char>(value));
    if(used == chunk_size) {
      visit(std::span<std::byte const>(chunk.data(), used));
      used = 0;
    }
  }
  if(used != 0) {
    visit(std::span<std::byte const>(chunk.data(), used));
  }
}

} // namespace checksum::detail

#endif // CHECKSUM_BYTE_RANGE_HPP

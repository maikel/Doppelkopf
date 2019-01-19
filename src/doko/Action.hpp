// Copyright (c) 2019 Maikel Nadolski
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef DOKO_ACTION_HPP
#define DOKO_ACTION_HPP

#include <array>
#include <cstddef>
#include <functional>
#include <numeric>

namespace doko {
/// This type represents the finite set of all possible colors of a card.
enum class color : unsigned char { diamonds, hearts, spades, clubs };

/// This type represents the finite set of all possible faces of a card.
enum class face : unsigned char { nine, jack, queen, king, ten, ace };

/// This player id is used to distinguish between the players of a running game.
enum class player_id : unsigned char { first, second, third, fourth };

/// @{
/// Returns an integral value of the argument which is usable to index into
/// arrays.
constexpr int to_integer(color color) noexcept {
  return static_cast<unsigned char>(color);
}

constexpr int to_integer(face face) noexcept {
  return static_cast<unsigned char>(face);
}

constexpr int to_integer(player_id player) noexcept {
  return static_cast<unsigned char>(player);
}

constexpr int to_integer(color color, face face) noexcept {
  return to_integer(face) + 6 * to_integer(color);
}
/// @}

constexpr player_id next_player(player_id player) noexcept {
  return player_id((to_integer(player) + 1) % 4);
}

class card {
public:
  card() = default;
  constexpr card(color color, face face,
                 player_id player = player_id()) noexcept
      : byte_((((std::byte(to_integer(player)) << 3) |
                std::byte(to_integer(face)))
               << 2) |
              std::byte(to_integer(color))) {}

  constexpr color color() const noexcept {
    return doko::color(byte_ & std::byte(0b0'00'000'11));
  }

  constexpr face face() const noexcept {
    return doko::face((byte_ & std::byte(0b0'00'111'00)) >> 2);
  }

  constexpr player_id player() const noexcept {
    return doko::player_id((byte_ & std::byte(0b0'11'000'00)) >> 5);
  }

  constexpr int eyes() const noexcept {
    constexpr std::array<int, 6> values{0, 2, 3, 4, 10, 11};
    return values[to_integer(face())];
  }

  constexpr explicit operator std::byte() const noexcept { return byte_; }

private:
  std::byte byte_;
};

constexpr int to_integer(card c) noexcept {
  return to_integer(c.color(), c.face());
}

constexpr card to_card(int n) noexcept {
  return card(color((n / 6) % 4), face(n % 6));
}

constexpr bool operator==(card lhs, card rhs) noexcept {
  return static_cast<std::byte>(lhs) == static_cast<std::byte>(rhs);
}

constexpr bool operator!=(card lhs, card rhs) noexcept { return !(lhs == rhs); }

enum class party : unsigned char { contra, re };

constexpr int to_integer(party party) { return static_cast<int>(party); }

class announcement {
public:
  announcement() = default;
  announcement(party party, player_id player = player_id())
      : byte_{std::byte(0b1'00'000'00) | (std::byte(to_integer(player)) << 5) |
              std::byte(to_integer(party))} {}

  party party() const noexcept {
    return doko::party(byte_ & std::byte(0b0'00'000'01));
  }

  player_id player() const noexcept {
    return doko::player_id((byte_ & std::byte(0b0'11'000'00)) >> 5);
  }

  constexpr explicit operator std::byte() const noexcept { return byte_; }

private:
  std::byte byte_;
};

bool operator==(announcement, announcement) noexcept;
bool operator!=(announcement, announcement) noexcept;

class action {
public:
  action() : byte_(static_cast<std::byte>(-1)) {}
  action(card card) noexcept : byte_(static_cast<std::byte>(card)) {}
  action(announcement bid) noexcept : byte_(static_cast<std::byte>(bid)) {}

  player_id player() const noexcept {
    return doko::player_id((byte_ & std::byte(0b0'11'000'00)) >> 5);
  }

  bool is_empty() const noexcept { return byte_ == static_cast<std::byte>(-1); }

  const announcement* as_bid() const noexcept {
    if ((byte_ & std::byte(0b1'00'000'00)) != std::byte(0)) {
      return reinterpret_cast<const announcement*>(&byte_);
    }
    return nullptr;
  }
  const card* as_card() const noexcept {
    if ((byte_ & std::byte(0b1'00'000'00)) == std::byte(0)) {
      return reinterpret_cast<const card*>(&byte_);
    }
    return nullptr;
  }

  constexpr explicit operator std::byte() const noexcept { return byte_; }

private:
  std::byte byte_;
};

bool operator==(action, action) noexcept;
bool operator!=(action, action) noexcept;

template <typename Visitor>
decltype(auto) visit(Visitor visitor, const doko::action& action) {
  if (const doko::card* card = action.as_card()) {
    std::invoke(visitor, *card);
  } else if (const doko::announcement* bid = action.as_bid()) {
    std::invoke(visitor, *bid);
  }
}

template <class... Ts> struct overload_set : Ts... { using Ts::operator()...; };
template <class... Ts> overload_set(Ts...)->overload_set<Ts...>;

} // namespace doko

#endif /* DOKO_ACTION_HPP */

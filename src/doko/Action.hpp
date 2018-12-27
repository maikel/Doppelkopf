// Copyright (c) 2018 Maikel Nadolski
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

#include <cstddef>

namespace doko {
enum class Color : unsigned char { diamonds, hearts, spades, clubs };
enum class Face : unsigned char { nine, jack, queen, king, ten, ace };
enum class PlayerId : unsigned char { first, second, third, fourth };

class Card {
public:
  Card() = default;
  Card(Color color, Face face, PlayerId player) noexcept;

  Color Color() const noexcept;
  Face Face() const noexcept;
  PlayerId Player() const noexcept;
  int Eyes() const noexcept;

private:
  std::byte data_;
};

bool operator==(Card, Card) noexcept;
bool operator!=(Card, Card) noexcept;

enum class Party : unsigned char { contra, re };

class Bid {
public:
  Bid() = default;
  Bid(Party party, PlayerId player);

  Party Party() const noexcept;
  PlayerId Player() const noexcept;

private:
  std::byte data_;
};

bool operator==(Bid, Bid) noexcept;
bool operator!=(Bid, Bid) noexcept;

class Action {
public:
  Action() = default;
  Action(Card card) noexcept;
  Action(Bid bid) noexcept;

  const Bid* AsBid() const noexcept;
  const Card* AsCard() const noexcept;

private:
  std::byte data_;
};

bool operator==(Action, Action) noexcept;
bool operator!=(Action, Action) noexcept;
} // namespace doko

#endif /* DOKO_ACTION_HPP */

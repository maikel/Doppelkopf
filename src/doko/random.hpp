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

#ifndef DOKO_RANDOM_HPP
#define DOKO_RANDOM_HPP

#include "doko/action.hpp"
#include "doko/game_rules.hpp"
#include "doko/static_vector.hpp"

#include <algorithm>
#include <iterator>
#include <random>

namespace doko {

template <typename InputIter, typename RandomNumberGenerator>
InputIter select_randomly(InputIter first, InputIter last,
                          RandomNumberGenerator& gen) {
  std::uniform_int_distribution<> uniform_dist(
      0, std::max(0L, std::distance(first, last) - 1));
  std::advance(first, uniform_dist(gen));
  return first;
}

template <typename Rng, typename Rules>
action select_random_legal_action(span<const action> history,
                                  span<const card> current_hand,
                                  span<const card> trick, Rng& gen,
                                  const Rules& rules) {
  static_vector<action, 13> legal =
      rules.legal_actions(current_hand, trick, history);
  if (legal[0].as_bid()) {
    return *select_randomly(legal.begin() + 1, legal.end(), gen);
  }
  return *select_randomly(legal.begin(), legal.end(), gen);
}

} // namespace doko

#endif
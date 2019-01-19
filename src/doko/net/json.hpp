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

#ifndef DOKO_SERVER_JSON_HPP
#define DOKO_SERVER_JSON_HPP

#include "doko/action.hpp"
#include "doko/static_vector.hpp"
#include "doko/game_rules.hpp"

#include <nlohmann/json.hpp>

#include <optional>

namespace doko {

void to_json(nlohmann::json& j, card card);
void from_json(const nlohmann::json& j, card& card);

void to_json(nlohmann::json& j, announcement bid);
void from_json(const nlohmann::json& j, announcement& bid);

void to_json(nlohmann::json& j, action action);
void from_json(const nlohmann::json& j, action& action);

void to_json(nlohmann::json& j, player_id player);
void from_json(const nlohmann::json& j, player_id& player);

template <typename T, int N>
void from_json(const nlohmann::json& j, static_vector<T, N>& vec) {
  for (int i = 0; i < j.size(); ++i) {
    vec.push_back(j[i].get<T>());
  }
}

template <typename T, int N>
void to_json(nlohmann::json& j, const static_vector<T, N>& vector) {
  j = nlohmann::json::array();
  for (auto&& x : vector) {
    j.push_back(x);
  }
}

void to_json(nlohmann::json& j, normal_game_rules);
void to_json(nlohmann::json& j, const marriage_rules& rules);
void to_json(nlohmann::json& j, const solo_rules& rules);
void to_json(nlohmann::json& j, const game_rules& rules);

void from_json(const nlohmann::json& j, solo_type& type);
void from_json(const nlohmann::json& j, game_rules& rules);

void to_json(nlohmann::json& j, const game_state_machine& game);
void to_json(nlohmann::json& j, const specialized_contract& contract);
void to_json(nlohmann::json& j, const declared_contract& contract);

void from_json(const nlohmann::json& j, game_state_machine& game);
void from_json(const nlohmann::json& j, specialized_contract& contract);
void from_json(const nlohmann::json& j, declared_contract& contract);

} // namespace doko

namespace std {
template <typename T> void from_json(const nlohmann::json& j, optional<T>& x) {
  if (j.is_null()) {
    x = {};
  } else {
    x = j.get<T>();
  }
}

template <typename T> void to_json(nlohmann::json& j, const optional<T>& x) {
  if (x) {
    j = *x;
  } else {
    j = nullptr;
  }
}
} // namespace std

#endif
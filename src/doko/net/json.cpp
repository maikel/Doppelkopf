
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

#include "doko/net/json.hpp"

#include <fmt/format.h>

namespace doko {
void from_json(const nlohmann::json& j, player_id& player) {
  player = player_id(j.get<int>());
}

void from_json(const nlohmann::json& j, card& card) {
  static const std::map<std::string, color> colors{
      {"diamonds", color::diamonds},
      {"hearts", color::hearts},
      {"spades", color::spades},
      {"clubs", color::clubs}};
  static const std::map<std::string, face> faces{
      {"nine", face::nine}, {"ten", face::ten},   {"ace", face::ace},
      {"king", face::king}, {"jack", face::jack}, {"queen", face::queen}};
  if (j.count("player")) {
    card = doko::card(colors.at(j.at("color").get<std::string>()),
                      faces.at(j.at("face").get<std::string>()),
                      j["player"].get<player_id>());
  } else {
    card = doko::card(colors.at(j.at("color").get<std::string>()),
                      faces.at(j.at("face").get<std::string>()));
  }
}

void from_json(const nlohmann::json& j, action& action) {
  if (j.count("color") && j.count("face")) {
    action = j.get<card>();
  } else if (j.count("party") && j.count("player")) {
    action = j.get<announcement>();
  } else {
    throw std::runtime_error("Parsing Error");
  }
}

void to_json(nlohmann::json& j, action action) {
  if (auto card = action.as_card()) {
    j = *card;
  } else if (auto bid = action.as_bid()) {
    j = *bid;
  } else {
    throw std::invalid_argument("Invalid Action");
  }
}

void from_json(const nlohmann::json& j, party& party) {
  std::string p = j.get<std::string>();
  if (p == "re") {
    party = party::re;
  } else if (p == "contra") {
    party = party::contra;
  } else {
    throw std::runtime_error("Parsing Error");
  }
}

void from_json(const nlohmann::json& j, announcement& bid) {
  bid =
      announcement(j.at("party").get<party>(), j.at("player").get<player_id>());
}

void to_json(nlohmann::json& j, card card) {
  constexpr std::array<const char*, 6> faces{"nine", "jack", "queen",
                                             "king", "ten",  "ace"};
  constexpr std::array<const char*, 4> colors{"diamonds", "hearts", "spades",
                                              "clubs"};
  j = {{"color", colors[to_integer(card.color())]},
       {"face", faces[to_integer(card.face())]},
       {"player", card.player()}};
}

void to_json(nlohmann::json& j, announcement bid) {
  constexpr std::array<const char*, 2> parties{"contra", "re"};
  j = {{"party", parties[to_integer(bid.party())]}, {"player", bid.player()}};
}

void to_json(nlohmann::json& j, player_id player) {
  j = static_cast<int>(player);
}

void to_json(nlohmann::json& j, normal_game_rules) { j = {{"name", "normal"}}; }

void to_json(nlohmann::json& j, const marriage_rules& rules) {
  j = {{"name", "marriage"}, {"bride", static_cast<int>(rules.bride)}};
}

void to_json(nlohmann::json& j, const solo_rules& rules) {
  static const std::array<const char*, 6> types{"jack",   "queen",  "diamonds",
                                                "hearts", "spades", "clubs"};
  j = {{"name", "solo"},
       {"solo_player", static_cast<int>(rules.solo_player)},
       {"solo_type", types[int(rules.type)]}};
}

void to_json(nlohmann::json& j, const game_rules& rules) {
  std::visit([&](const auto& r) { to_json(j, r); }, rules);
}

void from_json(const nlohmann::json& j, solo_type& type) {
  static const std::map<std::string, solo_type> map{
      {"jack", solo_type::jack},         {"queen", solo_type::queen},
      {"diamonds", solo_type::diamonds}, {"hearts", solo_type::hearts},
      {"spades", solo_type::spades},     {"clubs", solo_type::clubs}};
  type = map.at(j.get<std::string>());
}

void from_json(const nlohmann::json& j, game_rules& rules) {
  std::string name = j.at("name").get<std::string>();
  if (name == "normal") {
    rules = normal_game_rules();
  } else if (name == "marriage") {
    rules = marriage_rules{j.at("bride").get<player_id>()};
  } else if (name == "solo") {
    rules = solo_rules{j.at("solo_player").get<player_id>(),
                       j.at("solo_type").get<solo_type>()};
  } else {
    throw std::runtime_error("Parsing Error");
  }
}

void to_json(nlohmann::json& j,
             const game_state_machine::declare_contracts& contracts) {
  j = {{"choices", contracts.choices}, {"player", contracts.state.player}};
}

void to_json(nlohmann::json& j,
             const game_state_machine::specialize_contracts& contracts) {
  j = {{"choices", contracts.choices}, {"player", contracts.state.player}};
}

void to_json(nlohmann::json& j, const game_state_machine::running& running) {
  std::array<std::optional<card>, 4> trick{};
  for (card card : running.state.trick) {
    trick[to_integer(card.player())] = card;
  }
  if (running.actions.size() == 0) {
    j = {{"trick", trick},
         {"player", running.state.player},
         {"rules", running.rules},
         {"last_action", nullptr}};
  } else {
    const action last = running.actions[running.actions.size() - 1];
    j = {{"trick", trick},
         {"player", running.state.player},
         {"rules", running.rules},
         {"last_action", last}};
  }
}

void to_json(nlohmann::json& j, const game_state_machine::score& scoring) {
  j = {{"eyes", scoring.eyes},
       {"score", scoring.score},
       {"actions", scoring.actions}};
}

void to_json(nlohmann::json& j, const game_state_machine& game) {
  nlohmann::json state;
  switch (game.state.index()) {
  case 0:
    j = {{"state_type", "declare_contracts"}};
    state = *std::get_if<game_state_machine::declare_contracts>(&game.state);
    j.insert(state.begin(), state.end());
    break;
  case 1:
    j = {{"state_type", "specialize_contracts"}};
    state = *std::get_if<game_state_machine::specialize_contracts>(&game.state);
    j.insert(state.begin(), state.end());
    break;
  case 2:
    j = {{"state_type", "running"}};
    state = *std::get_if<game_state_machine::running>(&game.state);
    j.insert(state.begin(), state.end());
    break;
  default:
    j = {{"state_type", "scoring"}};
    state = std::get<game_state_machine::score>(game.state);
    j.insert(state.begin(), state.end());
  }
  j["initial_player"] = game.first_player;
}

void to_json(nlohmann::json& j, const specialized_contract& contract) {
  j = {{"rules", contract.rules}, {"player", contract.player}};
}

void to_json(nlohmann::json& j, const healthiness& h) {
  if (h == healthiness::healthy) {
    j = "healthy";
  } else {
    j = "reservation";
  }
}

void to_json(nlohmann::json& j, const declared_contract& contract) {
  j = {{"health", contract.health}, {"player", contract.player}};
}

void from_json(const nlohmann::json& j, healthiness& health) {
  if (j.get<std::string>() == "healthy") {
    health = healthiness::healthy;
  } else if (j.get<std::string>() == "reservation") {
    health = healthiness::reservation;
  } else {
    throw std::invalid_argument(
        fmt::format("Failed to parse healthiness. Input: {}", j.dump()));
  }
}

void from_json(const nlohmann::json& j, specialized_contract& contract) {
  contract.rules = j.at("rules").get<game_rules>();
  contract.player = j.at("player").get<player_id>();
}

void from_json(const nlohmann::json& j, declared_contract& contract) {
  contract.health = j.at("health").get<healthiness>();
  contract.player = j.at("player").get<player_id>();
}

} // namespace doko
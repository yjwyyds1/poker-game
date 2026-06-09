#define _WIN32_WINNT 0x0A00
#include "lib/httplib.h"
#include "lib/json.hpp"
#include "core/game_engine.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <random>

using json = nlohmann::json;

static std::mt19937 rng(std::random_device{}());

json card_to_json(const Card& c) {
    return {{"rank", std::string(1, rank_char(c.rank))},
            {"suit", std::string(1, suit_char(c.suit))}};
}

std::string phase_to_string(Phase p) {
    switch (p) {
        case Phase::PreFlop: return "preflop";
        case Phase::Flop:    return "flop";
        case Phase::Turn:    return "turn";
        case Phase::River:   return "river";
        case Phase::Showdown:return "showdown";
    }
    return "unknown";
}

json game_state_to_json(GameEngine& game, int you_index) {
    json j;
    j["phase"] = phase_to_string(game.phase());
    j["pot"] = game.pot();
    j["current_bet"] = game.current_bet();
    j["min_raise"] = game.min_raise();
    j["dealer_index"] = game.dealer_index();
    j["current_player"] = game.current_player();
    j["hand_over"] = game.is_hand_over();

    json community = json::array();
    for (const auto& c : game.community())
        community.push_back(card_to_json(c));
    j["community"] = community;

    json players = json::array();
    for (int i = 0; i < (int)game.players().size(); ++i) {
        const auto& p = game.players()[i];
        json pj;
        pj["index"] = i;
        pj["name"] = p.name;
        pj["chips"] = p.chips;
        pj["bet"] = p.bet_amount;
        pj["total_bet"] = p.total_bet;
        pj["folded"] = p.folded;
        pj["is_all_in"] = p.is_all_in;
        pj["is_you"] = (i == you_index);
        pj["is_dealer"] = (i == game.dealer_index());

        if (i == you_index || game.is_hand_over()) {
            json hole = json::array();
            for (const auto& c : p.hole_cards)
                hole.push_back(card_to_json(c));
            pj["hole_cards"] = hole;
        } else {
            pj["hole_cards"] = json::array();
        }

        if (game.is_hand_over() && !p.folded) {
            auto hand = p.best_hand(game.community());
            pj["best_hand"] = hand.to_string();
        }

        players.push_back(pj);
    }
    j["players"] = players;

    {
        auto actions = json::array();
        if (!game.is_hand_over() && game.current_player() == you_index) {
            for (auto a : game.allowed_actions(you_index)) {
                switch (a) {
                    case ActionType::Fold:  actions.push_back("fold"); break;
                    case ActionType::Check: actions.push_back("check"); break;
                    case ActionType::Call:  actions.push_back("call"); break;
                    case ActionType::Raise: actions.push_back("raise"); break;
                    case ActionType::AllIn: actions.push_back("allin"); break;
                }
            }
        }
        j["allowed_actions"] = actions;
    }

    if (game.is_hand_over()) {
        auto winners = game.get_winners();
        json wj = json::array();
        for (const auto& w : winners) {
            wj.push_back({{"player", w.player_index},
                          {"amount", w.won_amount},
                          {"hand", w.description}});
        }
        j["winners"] = wj;
    }

    return j;
}

void bot_play(GameEngine& game) {
    int idx = game.current_player();
    auto allowed = game.allowed_actions(idx);
    const auto& p = game.players()[idx];

    Action act;
    act.type = ActionType::Fold;

    bool can_check = false, can_call = false, can_raise = false;
    int call_amount = 0;
    for (auto a : allowed) {
        if (a == ActionType::Check) can_check = true;
        if (a == ActionType::Call) { can_call = true; call_amount = game.current_bet() - p.bet_amount; }
        if (a == ActionType::Raise) can_raise = true;
    }

    int chips = p.chips;
    int pot = game.pot();
    int r = std::uniform_int_distribution<>(0, 99)(rng);

    if (can_check && can_raise) {
        if (r < 50) act.type = ActionType::Check;
        else if (r < 80) {
            act.type = ActionType::Raise;
            act.amount = std::min(game.min_raise() * 2, chips);
        } else act.type = ActionType::Check;
    }
    else if (can_check) {
        act.type = ActionType::Check;
    }
    else if (can_call) {
        float ratio = (float)call_amount / (float)(pot + call_amount);
        if (r < 60 || ratio < 0.3f) {
            if (can_raise && r < 70) {
                act.type = ActionType::Raise;
                act.amount = std::min(game.min_raise(), chips);
            } else {
                act.type = ActionType::Call;
            }
        } else if (r < 85) {
            act.type = ActionType::Fold;
        } else {
            act.type = ActionType::Call;
        }
    }

    game.player_action(idx, act);
}

int main() {
    httplib::Server svr;

    svr.set_mount_point("/", "web");

    svr.WebSocket("/ws", [](const httplib::Request&, httplib::ws::WebSocket& ws) {
        GameEngine game(1, 2);
        int you_index = 0;
        bool waiting_for_new_hand = false;

        game.add_player("You", 200);
        game.add_player("Bot-Alpha", 200);
        game.add_player("Bot-Bravo", 200);
        game.add_player("Bot-Charlie", 200);
        game.start_hand();

        auto send_state = [&]() {
            json state = game_state_to_json(game, you_index);
            ws.send(state.dump());
        };

        send_state();

        std::string msg;
        while (ws.read(msg)) {
            if (msg.empty()) continue;

            if (waiting_for_new_hand) {
                try {
                    auto j = json::parse(msg);
                    if (j.value("type", "") == "new_hand") {
                        game.start_hand();
                        waiting_for_new_hand = false;
                    }
                } catch (...) {}
                send_state();
                continue;
            }

            try {
                auto j = json::parse(msg);
                std::string action = j.value("action", "");
                Action act;
                if (action == "fold") act.type = ActionType::Fold;
                else if (action == "check") act.type = ActionType::Check;
                else if (action == "call") act.type = ActionType::Call;
                else if (action == "raise") {
                    act.type = ActionType::Raise;
                    act.amount = j.value("amount", game.min_raise());
                }
                else if (action == "allin") act.type = ActionType::AllIn;
                else continue;

                auto result = game.player_action(you_index, act);
                if (result == ActionResult::Invalid) {
                    send_state();
                    continue;
                }

                while (!game.is_hand_over() && game.current_player() != you_index) {
                    bot_play(game);
                }

                if (game.is_hand_over()) {
                    waiting_for_new_hand = true;
                }

                send_state();
            } catch (...) {}
        }
    });

    std::cout << "Poker server running at http://localhost:8080\n";
    svr.listen("0.0.0.0", 8080);

    return 0;
}

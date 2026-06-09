#include "core/game_engine.hpp"
#include <iostream>
#include <cassert>

bool bot_play(GameEngine& game) {
    int idx = game.current_player();
    auto allowed = game.allowed_actions(idx);
    if (allowed.empty()) {
        std::cerr << "ERROR: bot " << idx << " has no actions!\n";
        return false;
    }
    const auto& p = game.players()[idx];

    Action act;
    act.type = ActionType::Fold;
    bool can_check = false, can_call = false, can_raise = false;
    for (auto a : allowed) {
        if (a == ActionType::Check) can_check = true;
        if (a == ActionType::Call) can_call = true;
        if (a == ActionType::Raise) can_raise = true;
    }
    if (can_check) act.type = ActionType::Check;
    else if (can_call) act.type = ActionType::Call;
    else act.type = ActionType::Fold;

    auto result = game.player_action(idx, act);
    std::cout << "  Bot[" << idx << "](" << p.name << ") " 
              << (act.type == ActionType::Check ? "check" : act.type == ActionType::Call ? "call" : "fold")
              << " -> result=" << (int)result
              << " phase=" << (int)game.phase()
              << " next=" << game.current_player()
              << " hand_over=" << game.is_hand_over() << "\n";
    return result != ActionResult::Invalid;
}

int main() {
    GameEngine game(1, 2);
    game.add_player("You", 200);
    game.add_player("Bot-A", 200);
    game.add_player("Bot-B", 200);
    game.add_player("Bot-C", 200);
    game.start_hand();

    int you = 0;
    int street = 0;

    while (!game.is_hand_over() && street++ < 10) {
        // Auto-play bots until it's your turn
        int safety = 0;
        while (!game.is_hand_over() && game.current_player() != you && safety++ < 200) {
            if (!bot_play(game)) { std::cerr << "BOT FAILED\n"; return 1; }
        }
        if (safety >= 200) { std::cerr << "INFINITE LOOP in pre-turn bot play\n"; return 1; }

        if (game.is_hand_over()) break;

        std::cout << "\n=== YOUR TURN: phase=" << (int)game.phase()
                  << " pot=" << game.pot() << " bet=" << game.current_bet() << " ===\n";
        auto allowed = game.allowed_actions(you);
        std::cout << "Allowed: ";
        for (auto a : allowed) {
            switch (a) {
                case ActionType::Fold: std::cout << "fold "; break;
                case ActionType::Check: std::cout << "check "; break;
                case ActionType::Call: std::cout << "call "; break;
                case ActionType::Raise: std::cout << "raise "; break;
                case ActionType::AllIn: std::cout << "allin "; break;
            }
        }
        std::cout << "\nCommunity: ";
        for (auto& c : game.community()) std::cout << c.to_string() << " ";
        std::cout << "\n";

        // Player calls
        Action act;
        bool has_check = false, has_call = false;
        for (auto a : allowed) {
            if (a == ActionType::Check) has_check = true;
            if (a == ActionType::Call) has_call = true;
        }
        act.type = has_check ? ActionType::Check : ActionType::Call;
        std::cout << "You " << (act.type == ActionType::Check ? "check" : "call") << "\n";

        auto result = game.player_action(you, act);
        std::cout << "Result: " << (int)result << " hand_over=" << game.is_hand_over() << "\n";
    }

    if (game.is_hand_over()) {
        std::cout << "\n=== HAND OVER ===\n";
        std::cout << "Community: ";
        for (auto& c : game.community()) std::cout << c.to_string() << " ";
        std::cout << "\n";
        auto winners = game.get_winners();
        for (auto& w : winners) {
            std::cout << game.players()[w.player_index].name << " wins " << w.won_amount
                      << " (" << w.description << ")\n";
        }
    }

    std::cout << "\nDone. Street count: " << street << "\n";
    return 0;
}

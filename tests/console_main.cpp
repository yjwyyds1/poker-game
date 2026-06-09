#include "core/game_engine.hpp"
#include <iostream>
#include <sstream>

std::string phase_name(Phase p) {
    switch (p) {
        case Phase::PreFlop: return "PreFlop";
        case Phase::Flop: return "Flop";
        case Phase::Turn: return "Turn";
        case Phase::River: return "River";
        case Phase::Showdown: return "Showdown";
    }
    return "?";
}

std::string action_name(ActionType a) {
    switch (a) {
        case ActionType::Fold: return "fold";
        case ActionType::Check: return "check";
        case ActionType::Call: return "call";
        case ActionType::Raise: return "raise";
        case ActionType::AllIn: return "allin";
    }
    return "?";
}

void print_state(GameEngine& game) {
    std::cout << "\n=== Phase: " << phase_name(game.phase())
              << " | Pot: " << game.pot()
              << " | Current bet: " << game.current_bet() << " ===\n";

    if (!game.community().empty()) {
        std::cout << "Board: ";
        for (const auto& c : game.community())
            std::cout << c.to_string() << " ";
        std::cout << "\n";
    }

    for (int i = 0; i < static_cast<int>(game.players().size()); ++i) {
        const auto& p = game.players()[i];
        std::cout << "  [" << i << "] " << (i == game.current_player() ? ">" : " ")
                  << p.name << " chips=" << p.chips
                  << " bet=" << p.bet_amount;
        if (p.folded) std::cout << " FOLDED";
        if (p.is_all_in) std::cout << " ALLIN";
        std::cout << "\n";
    }

    auto actions = game.allowed_actions(game.current_player());
    std::cout << "Allowed: ";
    for (auto a : actions) std::cout << action_name(a) << " ";
    std::cout << "\n";
}

int main() {
    GameEngine game(1, 2);
    game.on_log = [](const std::string& msg) {
        std::cout << "  " << msg << "\n";
    };

    game.add_player("Alice", 100);
    game.add_player("Bob", 100);
    game.add_player("Charlie", 100);

    game.start_hand();
    std::string line;

    while (!game.is_hand_over()) {
        print_state(game);

        const auto& p = game.players()[game.current_player()];
        std::cout << "\n" << p.name << "'s hand: ";
        if (p.hole_cards.size() >= 2)
            std::cout << p.hole_cards[0].to_string() << " " << p.hole_cards[1].to_string();
        std::cout << "\nAction: ";

        std::getline(std::cin, line);
        std::istringstream iss(line);
        std::string cmd;
        int amount = 0;
        iss >> cmd >> amount;

        Action action;
        if (cmd == "f") action.type = ActionType::Fold;
        else if (cmd == "c") action.type = ActionType::Call;
        else if (cmd == "k") action.type = ActionType::Check;
        else if (cmd == "r") { action.type = ActionType::Raise; action.amount = amount; }
        else if (cmd == "a") { action.type = ActionType::AllIn; }
        else { std::cout << "Unknown command\n"; continue; }

        auto result = game.player_action(game.current_player(), action);
        if (result == ActionResult::Invalid)
            std::cout << "Invalid action!\n";
        else if (result == ActionResult::HandOver) {
            std::cout << "\n=== HAND OVER ===\n";
            break;
        }
    }

    if (game.is_hand_over()) {
        auto winners = game.get_winners();
        std::cout << "\n=== RESULTS ===\n";
        for (int i = 0; i < static_cast<int>(game.players().size()); ++i) {
            const auto& p = game.players()[i];
            auto hand = p.best_hand(game.community());
            std::cout << p.name << ": " << p.hole_cards[0].to_string()
                      << " " << p.hole_cards[1].to_string();
            if (!p.folded) std::cout << " -> " << hand.to_string();
            std::cout << "\n";
        }
        std::cout << "\nBoard: ";
        for (const auto& c : game.community())
            std::cout << c.to_string() << " ";
        std::cout << "\n";

        for (const auto& w : winners) {
            std::cout << game.players()[w.player_index].name << " wins " << w.won_amount
                      << " with " << w.description << "\n";
        }
    }

    return 0;
}

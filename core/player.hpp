#pragma once
#include "core/card.hpp"
#include "core/hand_evaluator.hpp"
#include <string>
#include <vector>

struct Player {
    std::string name;
    int chips;
    int bet_amount = 0;       // Chips bet in current round
    int total_bet = 0;        // Total bet this hand
    std::vector<Card> hole_cards; // 2 cards
    bool folded = false;
    bool is_all_in = false;
    bool acted = false;

    Player(std::string n, int c) : name(std::move(n)), chips(c) {}

    void reset_for_hand();
    void reset_for_round();
    HandRank best_hand(const std::vector<Card>& community) const;
    std::string to_string() const;
};

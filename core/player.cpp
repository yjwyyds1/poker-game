#include "core/player.hpp"

void Player::reset_for_hand() {
    hole_cards.clear();
    folded = false;
    is_all_in = false;
    acted = false;
    bet_amount = 0;
    total_bet = 0;
}

void Player::reset_for_round() {
    bet_amount = 0;
    acted = false;
}

HandRank Player::best_hand(const std::vector<Card>& community) const {
    std::vector<Card> all = hole_cards;
    all.insert(all.end(), community.begin(), community.end());
    return HandEvaluator::evaluate(all);
}

std::string Player::to_string() const {
    std::string s = name + " [" + std::to_string(chips) + "] ";
    if (folded) return s + "(folded)";
    if (hole_cards.size() == 2)
        s += hole_cards[0].to_string() + " " + hole_cards[1].to_string();
    return s;
}

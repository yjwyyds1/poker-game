#pragma once
#include "core/card.hpp"
#include <vector>
#include <random>

class Deck {
public:
    Deck();
    void shuffle();
    void reset();
    Card deal();
    size_t remaining() const { return cards_.size() - top_; }

private:
    std::vector<Card> cards_;
    size_t top_ = 0;
    std::mt19937 rng_;
};

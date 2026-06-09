#include "core/deck.hpp"
#include <algorithm>
#include <chrono>

Deck::Deck() : rng_(static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count())) {
    reset();
}

void Deck::reset() {
    cards_.clear();
    for (uint8_t i = 0; i < 52; ++i)
        cards_.emplace_back(i);
    top_ = 0;
}

void Deck::shuffle() {
    std::shuffle(cards_.begin(), cards_.end(), rng_);
    top_ = 0;
}

Card Deck::deal() {
    return cards_[top_++];
}

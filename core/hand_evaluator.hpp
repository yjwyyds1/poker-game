#pragma once
#include "core/card.hpp"
#include <cstdint>
#include <array>
#include <string>
#include <vector>

enum class HandType : uint8_t {
    HighCard = 0,
    OnePair,
    TwoPair,
    ThreeOfAKind,
    Straight,
    Flush,
    FullHouse,
    FourOfAKind,
    StraightFlush
};

struct HandRank {
    HandType type = HandType::HighCard;
    uint8_t ranks[5] = {0,0,0,0,0}; // Sorted high-to-low, for tiebreaking

    auto operator<=>(const HandRank&) const = default;
    std::string to_string() const;
};

class HandEvaluator {
public:
    // Evaluate best 5-card hand from 5, 6, or 7 cards
    static HandRank evaluate(const std::vector<Card>& cards);
    static HandRank evaluate_5(const std::array<Card, 5>& cards);

    static const char* hand_type_name(HandType t);
};

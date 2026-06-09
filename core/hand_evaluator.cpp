#include "core/hand_evaluator.hpp"
#include <algorithm>
#include <numeric>

static void sort_by_rank(std::array<Card, 5>& cards) {
    std::sort(cards.begin(), cards.end(),
        [](const Card& a, const Card& b) {
            return static_cast<uint8_t>(a.rank) > static_cast<uint8_t>(b.rank);
        });
}

HandRank HandEvaluator::evaluate_5(const std::array<Card, 5>& cards) {
    auto sorted = cards;
    sort_by_rank(sorted);

    uint8_t ranks[5];
    uint8_t suits[5];
    for (int i = 0; i < 5; ++i) {
        ranks[i] = static_cast<uint8_t>(sorted[i].rank);
        suits[i] = static_cast<uint8_t>(sorted[i].suit);
    }

    bool flush = (suits[0] == suits[1] && suits[0] == suits[2] &&
                  suits[0] == suits[3] && suits[0] == suits[4]);

    bool straight = false;
    uint8_t straight_high = 0;
    // Normal straight
    if (ranks[0] - ranks[4] == 4 &&
        ranks[0] != ranks[1] && ranks[1] != ranks[2] &&
        ranks[2] != ranks[3] && ranks[3] != ranks[4]) {
        straight = true;
        straight_high = ranks[0];
    }
    // Wheel: A-2-3-4-5 (ranks: 12,3,2,1,0 after sort)
    if (ranks[0] == 12 && ranks[1] == 3 && ranks[2] == 2 && ranks[3] == 1 && ranks[4] == 0) {
        straight = true;
        straight_high = 3; // 5-high straight
    }

    // Count duplicates
    int count[13] = {};
    for (int i = 0; i < 5; ++i) count[ranks[i]]++;

    int quads = -1, trips = -1, pair1 = -1, pair2 = -1;
    for (int r = 12; r >= 0; --r) {
        if (count[r] == 4) quads = r;
        else if (count[r] == 3) trips = r;
        else if (count[r] == 2) {
            if (pair1 < 0) pair1 = r;
            else pair2 = r;
        }
    }

    // Get kickers sorted high-to-low (cards not in any pair/trips/quads)
    uint8_t kickers[5];
    int k = 0;
    for (int i = 0; i < 5; ++i) {
        int r = static_cast<uint8_t>(sorted[i].rank);
        if (count[r] == 1) kickers[k++] = r;
    }

    HandRank result;

    if (straight && flush) {
        result.type = HandType::StraightFlush;
        result.ranks[0] = straight_high;
    } else if (quads >= 0) {
        result.type = HandType::FourOfAKind;
        result.ranks[0] = static_cast<uint8_t>(quads);
        result.ranks[1] = kickers[0];
    } else if (trips >= 0 && pair1 >= 0) {
        result.type = HandType::FullHouse;
        result.ranks[0] = static_cast<uint8_t>(trips);
        result.ranks[1] = static_cast<uint8_t>(pair1);
    } else if (flush) {
        result.type = HandType::Flush;
        for (int i = 0; i < 5; ++i) result.ranks[i] = ranks[i];
    } else if (straight) {
        result.type = HandType::Straight;
        result.ranks[0] = straight_high;
    } else if (trips >= 0) {
        result.type = HandType::ThreeOfAKind;
        result.ranks[0] = static_cast<uint8_t>(trips);
        result.ranks[1] = kickers[0];
        result.ranks[2] = kickers[1];
    } else if (pair1 >= 0 && pair2 >= 0) {
        result.type = HandType::TwoPair;
        result.ranks[0] = static_cast<uint8_t>(pair1);
        result.ranks[1] = static_cast<uint8_t>(pair2);
        result.ranks[2] = kickers[0];
    } else if (pair1 >= 0) {
        result.type = HandType::OnePair;
        result.ranks[0] = static_cast<uint8_t>(pair1);
        result.ranks[1] = kickers[0];
        result.ranks[2] = kickers[1];
        result.ranks[3] = kickers[2];
    } else {
        result.type = HandType::HighCard;
        for (int i = 0; i < 5; ++i) result.ranks[i] = ranks[i];
    }

    return result;
}

HandRank HandEvaluator::evaluate(const std::vector<Card>& cards) {
    // Generate all C(n,5) combinations and pick best
    int n = static_cast<int>(cards.size());
    HandRank best;

    for (int a = 0; a < n - 4; ++a)
    for (int b = a + 1; b < n - 3; ++b)
    for (int c = b + 1; c < n - 2; ++c)
    for (int d = c + 1; d < n - 1; ++d)
    for (int e = d + 1; e < n; ++e) {
        std::array<Card, 5> combo = {cards[a], cards[b], cards[c], cards[d], cards[e]};
        HandRank rank = evaluate_5(combo);
        if (rank > best) best = rank;
    }

    return best;
}

const char* HandEvaluator::hand_type_name(HandType t) {
    switch (t) {
        case HandType::HighCard: return "High Card";
        case HandType::OnePair: return "One Pair";
        case HandType::TwoPair: return "Two Pair";
        case HandType::ThreeOfAKind: return "Three of a Kind";
        case HandType::Straight: return "Straight";
        case HandType::Flush: return "Flush";
        case HandType::FullHouse: return "Full House";
        case HandType::FourOfAKind: return "Four of a Kind";
        case HandType::StraightFlush: return "Straight Flush";
    }
    return "Unknown";
}

std::string HandRank::to_string() const {
    return HandEvaluator::hand_type_name(type);
}

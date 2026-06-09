#include "core/card.hpp"
#include "core/deck.hpp"
#include "core/hand_evaluator.hpp"
#include "core/player.hpp"
#include "core/game_engine.hpp"
#include <iostream>
#include <cassert>

static int tests_passed = 0;
static int tests_failed = 0;

void check(const std::string& name, bool condition) {
    if (condition) {
        tests_passed++;
        std::cout << "  PASS: " << name << "\n";
    } else {
        tests_failed++;
        std::cout << "  FAIL: " << name << "\n";
    }
}

Card C(int id) { return Card(static_cast<uint8_t>(id)); }

// Helper: Ah = 51 (ace of hearts = 12 + 2*13), Ks = 50, etc.
// Deuce=0, Trey=1, ..., Ace=12
// Clubs=0, Diamonds=1, Hearts=2, Spades=3
// id = rank + suit*13

Card mk(Rank r, Suit s) { return Card(r, s); }

void test_card() {
    std::cout << "== Card tests ==\n";
    Card c(Rank::Ace, Suit::Spades);
    check("Ace of Spades id=51", c.id() == 51);
    check("Ace of Spades string", c.to_string() == "As");
    check("rank_char", rank_char(Rank::Ten) == 'T');
    check("suit_char", suit_char(Suit::Hearts) == 'h');
}

void test_deck() {
    std::cout << "== Deck tests ==\n";
    Deck d;
    check("full deck", d.remaining() == 52);
    d.shuffle();
    check("still 52 after shuffle", d.remaining() == 52);
    Card c1 = d.deal();
    Card c2 = d.deal();
    check("51 after dealing 2", d.remaining() == 50);
    d.reset();
    check("52 after reset", d.remaining() == 52);
}

void test_evaluator() {
    std::cout << "== Hand Evaluator tests ==\n";

    // Royal flush
    {
        std::array<Card, 5> hand = {
            mk(Rank::Ace, Suit::Hearts), mk(Rank::King, Suit::Hearts),
            mk(Rank::Queen, Suit::Hearts), mk(Rank::Jack, Suit::Hearts),
            mk(Rank::Ten, Suit::Hearts)
        };
        auto r = HandEvaluator::evaluate_5(hand);
        check("Royal flush", r.type == HandType::StraightFlush && r.ranks[0] == 12);
    }

    // Straight flush (9h 8h 7h 6h 5h)
    {
        std::array<Card, 5> hand = {
            mk(Rank::Nine, Suit::Hearts), mk(Rank::Eight, Suit::Hearts),
            mk(Rank::Seven, Suit::Hearts), mk(Rank::Six, Suit::Hearts),
            mk(Rank::Five, Suit::Hearts)
        };
        auto r = HandEvaluator::evaluate_5(hand);
        check("9-high straight flush", r.type == HandType::StraightFlush && r.ranks[0] == 7);
    }

    // Four of a kind
    {
        std::array<Card, 5> hand = {
            mk(Rank::King, Suit::Hearts), mk(Rank::King, Suit::Spades),
            mk(Rank::King, Suit::Diamonds), mk(Rank::King, Suit::Clubs),
            mk(Rank::Ace, Suit::Hearts)
        };
        auto r = HandEvaluator::evaluate_5(hand);
        check("Four Kings", r.type == HandType::FourOfAKind && r.ranks[0] == 11);
    }

    // Full house
    {
        std::array<Card, 5> hand = {
            mk(Rank::Ace, Suit::Hearts), mk(Rank::Ace, Suit::Spades),
            mk(Rank::Ace, Suit::Diamonds), mk(Rank::King, Suit::Hearts),
            mk(Rank::King, Suit::Spades)
        };
        auto r = HandEvaluator::evaluate_5(hand);
        check("Aces full of Kings", r.type == HandType::FullHouse && r.ranks[0] == 12 && r.ranks[1] == 11);
    }

    // Flush
    {
        std::array<Card, 5> hand = {
            mk(Rank::Ace, Suit::Clubs), mk(Rank::Jack, Suit::Clubs),
            mk(Rank::Nine, Suit::Clubs), mk(Rank::Five, Suit::Clubs),
            mk(Rank::Trey, Suit::Clubs)
        };
        auto r = HandEvaluator::evaluate_5(hand);
        check("Ace-high flush", r.type == HandType::Flush);
    }

    // Straight
    {
        std::array<Card, 5> hand = {
            mk(Rank::Nine, Suit::Hearts), mk(Rank::Eight, Suit::Clubs),
            mk(Rank::Seven, Suit::Spades), mk(Rank::Six, Suit::Diamonds),
            mk(Rank::Five, Suit::Hearts)
        };
        auto r = HandEvaluator::evaluate_5(hand);
        check("9-high straight", r.type == HandType::Straight && r.ranks[0] == 7);
    }

    // Wheel straight (A-2-3-4-5)
    {
        std::array<Card, 5> hand = {
            mk(Rank::Ace, Suit::Hearts), mk(Rank::Deuce, Suit::Clubs),
            mk(Rank::Trey, Suit::Spades), mk(Rank::Four, Suit::Diamonds),
            mk(Rank::Five, Suit::Hearts)
        };
        auto r = HandEvaluator::evaluate_5(hand);
        check("Wheel straight (5-high)", r.type == HandType::Straight && r.ranks[0] == 3);
    }

    // Three of a kind
    {
        std::array<Card, 5> hand = {
            mk(Rank::Queen, Suit::Hearts), mk(Rank::Queen, Suit::Spades),
            mk(Rank::Queen, Suit::Diamonds), mk(Rank::Ace, Suit::Hearts),
            mk(Rank::King, Suit::Spades)
        };
        auto r = HandEvaluator::evaluate_5(hand);
        check("Three Queens", r.type == HandType::ThreeOfAKind && r.ranks[0] == 10);
    }

    // Two pair
    {
        std::array<Card, 5> hand = {
            mk(Rank::Ace, Suit::Hearts), mk(Rank::Ace, Suit::Spades),
            mk(Rank::King, Suit::Diamonds), mk(Rank::King, Suit::Hearts),
            mk(Rank::Queen, Suit::Spades)
        };
        auto r = HandEvaluator::evaluate_5(hand);
        check("Aces and Kings", r.type == HandType::TwoPair && r.ranks[0] == 12 && r.ranks[1] == 11);
    }

    // One pair
    {
        std::array<Card, 5> hand = {
            mk(Rank::Ace, Suit::Hearts), mk(Rank::Ace, Suit::Spades),
            mk(Rank::King, Suit::Diamonds), mk(Rank::Queen, Suit::Hearts),
            mk(Rank::Jack, Suit::Spades)
        };
        auto r = HandEvaluator::evaluate_5(hand);
        check("Pair of Aces", r.type == HandType::OnePair && r.ranks[0] == 12);
    }

    // High card
    {
        std::array<Card, 5> hand = {
            mk(Rank::Ace, Suit::Hearts), mk(Rank::King, Suit::Spades),
            mk(Rank::Nine, Suit::Diamonds), mk(Rank::Five, Suit::Hearts),
            mk(Rank::Trey, Suit::Spades)
        };
        auto r = HandEvaluator::evaluate_5(hand);
        check("Ace-high", r.type == HandType::HighCard && r.ranks[0] == 12);
    }

    // Comparison tests
    {
        auto royal = HandEvaluator::evaluate_5({
            mk(Rank::Ace, Suit::Hearts), mk(Rank::King, Suit::Hearts),
            mk(Rank::Queen, Suit::Hearts), mk(Rank::Jack, Suit::Hearts),
            mk(Rank::Ten, Suit::Hearts)
        });
        auto quads = HandEvaluator::evaluate_5({
            mk(Rank::Ace, Suit::Hearts), mk(Rank::Ace, Suit::Spades),
            mk(Rank::Ace, Suit::Diamonds), mk(Rank::Ace, Suit::Clubs),
            mk(Rank::King, Suit::Hearts)
        });
        check("Royal > Quads", royal > quads);
        check("Quads < Royal", quads < royal);
    }

    // 7-card evaluation
    {
        std::vector<Card> cards = {
            mk(Rank::Ace, Suit::Hearts), mk(Rank::King, Suit::Hearts),
            mk(Rank::Queen, Suit::Hearts), mk(Rank::Jack, Suit::Hearts),
            mk(Rank::Ten, Suit::Hearts), mk(Rank::Deuce, Suit::Clubs),
            mk(Rank::Trey, Suit::Diamonds)
        };
        auto r = HandEvaluator::evaluate(cards);
        check("7-card finds royal flush", r.type == HandType::StraightFlush);
    }
}

void test_game_engine() {
    std::cout << "== Game Engine tests ==\n";

    GameEngine game(1, 2);
    game.add_player("Alice", 100);
    game.add_player("Bob", 100);

    // Test blind posting
    game.start_hand();
    check("Starts in PreFlop", game.phase() == Phase::PreFlop);
    check("2 players dealt", game.players()[0].hole_cards.size() == 2);
    check("Pot has blinds", game.pot() == 3); // 1 + 2

    // Test fold preflop - Bob folds
    // Need to figure out who acts first
}

int main() {
    std::cout << "Poker Tests\n===========\n\n";

    test_card();
    test_deck();
    test_evaluator();
    test_game_engine();

    std::cout << "\n===========\n";
    std::cout << "Passed: " << tests_passed << ", Failed: " << tests_failed << "\n";
    return tests_failed > 0 ? 1 : 0;
}

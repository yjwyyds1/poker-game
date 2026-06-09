#pragma once
#include <cstdint>
#include <string>

enum class Suit : uint8_t { Clubs = 0, Diamonds = 1, Hearts = 2, Spades = 3 };
enum class Rank : uint8_t { Deuce = 0, Trey, Four, Five, Six, Seven, Eight, Nine, Ten, Jack, Queen, King, Ace };

struct Card {
    Rank rank;
    Suit suit;

    Card() : rank(Rank::Deuce), suit(Suit::Clubs) {}
    Card(Rank r, Suit s) : rank(r), suit(s) {}
    explicit Card(uint8_t id); // 0-51

    uint8_t id() const { return static_cast<uint8_t>(rank) + static_cast<uint8_t>(suit) * 13; }
    std::string to_string() const;

    bool operator==(const Card& other) const { return id() == other.id(); }
    bool operator!=(const Card& other) const { return id() != other.id(); }
};

const char* rank_name(Rank r);
const char* suit_name(Suit s);
char rank_char(Rank r);
char suit_char(Suit s);

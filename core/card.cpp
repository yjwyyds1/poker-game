#include "core/card.hpp"

Card::Card(uint8_t id) {
    rank = static_cast<Rank>(id % 13);
    suit = static_cast<Suit>(id / 13);
}

const char* rank_name(Rank r) {
    static const char* names[] = {"2","3","4","5","6","7","8","9","10","J","Q","K","A"};
    return names[static_cast<int>(r)];
}

const char* suit_name(Suit s) {
    static const char* names[] = {"Clubs","Diamonds","Hearts","Spades"};
    return names[static_cast<int>(s)];
}

char rank_char(Rank r) {
    static const char chars[] = {'2','3','4','5','6','7','8','9','T','J','Q','K','A'};
    return chars[static_cast<int>(r)];
}

char suit_char(Suit s) {
    static const char chars[] = {'c','d','h','s'};
    return chars[static_cast<int>(s)];
}

std::string Card::to_string() const {
    return std::string(rank_name(rank)) + suit_char(suit);
}

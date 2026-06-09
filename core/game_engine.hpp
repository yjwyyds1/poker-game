#pragma once
#include "core/card.hpp"
#include "core/deck.hpp"
#include "core/player.hpp"
#include "core/hand_evaluator.hpp"
#include <vector>
#include <string>
#include <functional>

enum class ActionType { Fold, Check, Call, Raise, AllIn };
enum class Phase { PreFlop, Flop, Turn, River, Showdown };
enum class ActionResult { Ok, Invalid, HandOver };

struct Action {
    ActionType type;
    int amount = 0; // Raise amount or 0
};

class GameEngine {
public:
    GameEngine(int small_blind = 1, int big_blind = 2);

    void add_player(const std::string& name, int chips);
    void start_hand();

    // Betting action by player index
    ActionResult player_action(int player_index, Action action);

    // State queries
    Phase phase() const { return phase_; }
    int pot() const;
    int current_bet() const { return current_bet_; }
    int min_raise() const { return min_raise_; }
    int dealer_index() const { return dealer_idx_; }
    int current_player() const;
    const std::vector<Player>& players() const { return players_; }
    const std::vector<Card>& community() const { return community_; }
    int active_player_count() const;
    bool is_hand_over() const { return hand_over_; }

    // Get allowed actions for current player
    std::vector<ActionType> allowed_actions(int player_index) const;

    // Get winner info after showdown
    struct WinnerInfo {
        int player_index;
        HandRank hand;
        int won_amount;
        std::string description;
    };
    std::vector<WinnerInfo> get_winners() const;

    // Callbacks for UI
    std::function<void(const std::string&)> on_log;
    std::function<void()> on_state_changed;

private:
    void deal_hole_cards();
    void deal_community(int count);
    void post_blinds();
    void next_phase();
    void advance_player();
    void collect_bets();
    void reset_betting_round();
    std::vector<int> get_side_pots(std::vector<int>& eligible) const;
    int next_active_player(int from) const;

    Deck deck_;
    std::vector<Player> players_;
    std::vector<Card> community_;
    int dealer_idx_ = -1;
    int small_blind_;
    int big_blind_;
    Phase phase_;
    int current_bet_ = 0;
    int min_raise_;
    int current_player_idx_ = -1;
    int last_raiser_idx_ = -1;
    int players_acted_ = 0;
    bool hand_over_ = false;

    void log(const std::string& msg);
};

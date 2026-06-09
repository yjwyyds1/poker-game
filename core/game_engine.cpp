#include "core/game_engine.hpp"
#include <algorithm>
#include <map>

GameEngine::GameEngine(int small_blind, int big_blind)
    : small_blind_(small_blind), big_blind_(big_blind), phase_(Phase::PreFlop) {}

void GameEngine::add_player(const std::string& name, int chips) {
    players_.emplace_back(name, chips);
}

void GameEngine::log(const std::string& msg) {
    if (on_log) on_log(msg);
}

int GameEngine::pot() const {
    int total = 0;
    for (const auto& p : players_) total += p.total_bet;
    return total;
}

int GameEngine::active_player_count() const {
    int count = 0;
    for (const auto& p : players_)
        if (!p.folded && !p.is_all_in) ++count;
    return count;
}

int GameEngine::current_player() const {
    return current_player_idx_;
}

int GameEngine::next_active_player(int from) const {
    int n = static_cast<int>(players_.size());
    for (int i = 1; i <= n; ++i) {
        int idx = (from + i) % n;
        if (!players_[idx].folded) return idx;
    }
    return from;
}

void GameEngine::start_hand() {
    deck_.reset();
    deck_.shuffle();
    community_.clear();
    hand_over_ = false;

    for (auto& p : players_) p.reset_for_hand();

    // Remove players with no chips
    players_.erase(std::remove_if(players_.begin(), players_.end(),
        [](const Player& p) { return p.chips <= 0; }), players_.end());

    if (players_.size() < 2) {
        log("Not enough players.");
        hand_over_ = true;
        return;
    }

    dealer_idx_ = next_active_player(dealer_idx_);
    log("--- New hand (dealer: " + players_[dealer_idx_].name + ") ---");

    deal_hole_cards();
    post_blinds();
    phase_ = Phase::PreFlop;
    current_bet_ = big_blind_;
    min_raise_ = big_blind_;

    // First to act preflop: player after big blind
    int bb = next_active_player(dealer_idx_);
    bb = next_active_player(bb); // skip SB, BB is next
    current_player_idx_ = next_active_player(bb);
    last_raiser_idx_ = current_player_idx_;
    players_acted_ = 0;

    if (active_player_count() <= 1) {
        // Everyone folded/all-in already
        collect_bets();
        hand_over_ = true;
        phase_ = Phase::Showdown;
    }
}

void GameEngine::deal_hole_cards() {
    for (int i = 0; i < 2; ++i)
        for (auto& p : players_) {
            if (p.chips > 0)
                p.hole_cards.push_back(deck_.deal());
        }
}

void GameEngine::deal_community(int count) {
    for (int i = 0; i < count; ++i)
        community_.push_back(deck_.deal());
}

void GameEngine::post_blinds() {
    int n = static_cast<int>(players_.size());
    int sb_idx = next_active_player(dealer_idx_);
    int bb_idx = next_active_player(sb_idx);

    auto post = [&](int idx, int amount) {
        int actual = std::min(amount, players_[idx].chips);
        players_[idx].chips -= actual;
        players_[idx].bet_amount = actual;
        players_[idx].total_bet = actual;
        players_[idx].acted = true;
        if (players_[idx].chips == 0) players_[idx].is_all_in = true;
        log(players_[idx].name + " posts " + std::to_string(actual) + " (" +
            (idx == sb_idx ? "SB" : "BB") + ")");
    };

    post(sb_idx, small_blind_);
    post(bb_idx, big_blind_);
    current_bet_ = players_[bb_idx].bet_amount;
}

ActionResult GameEngine::player_action(int player_index, Action action) {
    if (hand_over_) return ActionResult::HandOver;
    if (player_index != current_player_idx_) return ActionResult::Invalid;

    auto& p = players_[player_index];
    if (p.folded || p.is_all_in) return ActionResult::Invalid;

    int to_call = current_bet_ - p.bet_amount;

    switch (action.type) {
    case ActionType::Fold:
        p.folded = true;
        p.acted = true;
        log(p.name + " folds");
        break;

    case ActionType::Check:
        if (to_call > 0) return ActionResult::Invalid;
        p.acted = true;
        log(p.name + " checks");
        break;

    case ActionType::Call: {
        int call_amt = std::min(to_call, p.chips);
        p.chips -= call_amt;
        p.bet_amount += call_amt;
        p.total_bet += call_amt;
        p.acted = true;
        if (p.chips == 0) p.is_all_in = true;
        log(p.name + " calls " + std::to_string(call_amt));
        break;
    }

    case ActionType::Raise: {
        if (action.amount < current_bet_ + min_raise_) return ActionResult::Invalid;
        int raise_total = std::min(action.amount, p.bet_amount + p.chips);
        int raise_diff = raise_total - p.bet_amount;
        if (raise_diff <= to_call) return ActionResult::Invalid;

        p.chips -= raise_diff;
        p.bet_amount = raise_total;
        p.total_bet += raise_diff;
        p.acted = true;
        last_raiser_idx_ = player_index;
        min_raise_ = p.bet_amount - current_bet_;
        current_bet_ = p.bet_amount;
        if (p.chips == 0) p.is_all_in = true;
        log(p.name + " raises to " + std::to_string(raise_total));
        break;
    }

    case ActionType::AllIn:
        if (p.chips == 0) return ActionResult::Invalid;
        {
            int total = p.bet_amount + p.chips;
            p.total_bet += p.chips;
            p.bet_amount = total;
            p.chips = 0;
            p.is_all_in = true;
            p.acted = true;
            if (total > current_bet_) {
                min_raise_ = total - current_bet_;
                current_bet_ = total;
                last_raiser_idx_ = player_index;
            }
            log(p.name + " goes all in for " + std::to_string(total) + " total");
        }
        break;
    }

    // Count active (non-folded, non-all-in) players
    int active = 0;
    for (const auto& pl : players_)
        if (!pl.folded && !pl.is_all_in) ++active;

    // If only 1 player remains, hand is over
    int not_folded = 0;
    for (const auto& pl : players_)
        if (!pl.folded) ++not_folded;
    if (not_folded <= 1) {
        collect_bets();
        hand_over_ = true;
        phase_ = Phase::Showdown;
        return ActionResult::HandOver;
    }

    // If everyone all-in or folded, run out the board
    if (active <= 1 && not_folded > 1) {
        // Run out remaining community cards
        while (community_.size() < 5) {
            if (community_.size() == 0) { phase_ = Phase::Flop; deal_community(3); }
            else if (community_.size() == 3) { phase_ = Phase::Turn; deal_community(1); }
            else { phase_ = Phase::River; deal_community(1); }
        }
        collect_bets();
        hand_over_ = true;
        phase_ = Phase::Showdown;
        return ActionResult::HandOver;
    }

    advance_player();

    // Check if betting round is over
    bool round_over = false;
    if (active == 0 || players_acted_ >= static_cast<int>(players_.size())) {
        // Check if everyone has acted and bets are equal
        bool all_equal = true;
        for (const auto& pl : players_) {
            if (!pl.folded && !pl.is_all_in && pl.bet_amount != current_bet_)
                all_equal = false;
        }
        if (all_equal && current_player_idx_ == last_raiser_idx_)
            round_over = true;
    }

    if (round_over) {
        collect_bets();
        next_phase();
        if (phase_ == Phase::Showdown) {
            hand_over_ = true;
            return ActionResult::HandOver;
        }
    }

    if (on_state_changed) on_state_changed();
    return ActionResult::Ok;
}

void GameEngine::advance_player() {
    current_player_idx_ = next_active_player(current_player_idx_);
    players_acted_++;
    if (players_[current_player_idx_].bet_amount == current_bet_)
        players_[current_player_idx_].acted = true;

    // Skip all-in and folded players
    while (players_[current_player_idx_].folded || players_[current_player_idx_].is_all_in) {
        current_player_idx_ = next_active_player(current_player_idx_);
        players_acted_++;
    }
}

void GameEngine::collect_bets() {
    // Bets are already tracked in total_bet, nothing to collect here
}

void GameEngine::reset_betting_round() {
    for (auto& p : players_) p.reset_for_round();
    current_bet_ = 0;
    min_raise_ = big_blind_;
    players_acted_ = 0;
}

void GameEngine::next_phase() {
    reset_betting_round();

    switch (phase_) {
    case Phase::PreFlop:
        phase_ = Phase::Flop;
        deal_community(3);
        log("--- Flop: " + community_[0].to_string() + " " +
            community_[1].to_string() + " " + community_[2].to_string() + " ---");
        current_player_idx_ = next_active_player(dealer_idx_);
        break;

    case Phase::Flop:
        phase_ = Phase::Turn;
        deal_community(1);
        log("--- Turn: " + community_[3].to_string() + " ---");
        current_player_idx_ = next_active_player(dealer_idx_);
        break;

    case Phase::Turn:
        phase_ = Phase::River;
        deal_community(1);
        log("--- River: " + community_[4].to_string() + " ---");
        current_player_idx_ = next_active_player(dealer_idx_);
        break;

    case Phase::River:
        phase_ = Phase::Showdown;
        log("--- Showdown ---");
        break;

    case Phase::Showdown:
        break;
    }

    last_raiser_idx_ = current_player_idx_;

    // Skip folded/all-in
    while (players_[current_player_idx_].folded || players_[current_player_idx_].is_all_in)
        current_player_idx_ = next_active_player(current_player_idx_);
}

std::vector<ActionType> GameEngine::allowed_actions(int player_index) const {
    std::vector<ActionType> actions;
    const auto& p = players_[player_index];
    if (p.folded || p.is_all_in) return actions;

    int to_call = current_bet_ - p.bet_amount;
    actions.push_back(ActionType::Fold);

    if (to_call == 0) {
        actions.push_back(ActionType::Check);
    } else {
        actions.push_back(ActionType::Call);
        if (p.chips > to_call) actions.push_back(ActionType::Raise);
    }

    if (p.chips > 0) actions.push_back(ActionType::AllIn);
    return actions;
}

std::vector<GameEngine::WinnerInfo> GameEngine::get_winners() const {
    std::vector<WinnerInfo> results;

    // Find players still in the hand
    std::vector<int> active;
    for (int i = 0; i < static_cast<int>(players_.size()); ++i)
        if (!players_[i].folded) active.push_back(i);

    if (active.empty()) return results;

    // If only one player, they win everything
    if (active.size() == 1) {
        WinnerInfo w;
        w.player_index = active[0];
        w.won_amount = pot();
        w.description = "Last player standing";
        results.push_back(w);
        return results;
    }

    // Evaluate hands
    std::vector<std::pair<int, HandRank>> hands;
    for (int idx : active)
        hands.push_back({idx, players_[idx].best_hand(community_)});

    // Sort by hand rank (best first)
    std::sort(hands.begin(), hands.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    // Side pot calculation
    // Sort players by total_bet ascending
    std::vector<int> all_in = active;
    std::sort(all_in.begin(), all_in.end(),
        [this](int a, int b) { return players_[a].total_bet < players_[b].total_bet; });

    int prev_bet = 0;
    for (int player_idx : all_in) {
        int bet = players_[player_idx].total_bet;
        if (bet <= prev_bet) continue;

        int pot_share = 0;
        int eligible = 0;
        for (const auto& p : players_) {
            int contrib = std::min(p.total_bet, bet) - std::min(p.total_bet, prev_bet);
            pot_share += std::max(0, contrib);
            if (!p.folded && p.total_bet >= bet) eligible++;
        }

        // Find best hand among eligible players for this pot
        int best_player = -1;
        HandRank best_hand;
        std::vector<int> tie_players;
        for (const auto& [idx, hand] : hands) {
            if (players_[idx].total_bet >= bet) {
                if (best_player < 0 || hand > best_hand) {
                    best_player = idx;
                    best_hand = hand;
                    tie_players = {idx};
                } else if (hand == best_hand) {
                    tie_players.push_back(idx);
                }
            }
        }

        int per_player = pot_share / static_cast<int>(tie_players.size());
        for (int idx : tie_players) {
            // Find or create winner info
            auto it = std::find_if(results.begin(), results.end(),
                [idx](const WinnerInfo& w) { return w.player_index == idx; });
            if (it == results.end()) {
                WinnerInfo w;
                w.player_index = idx;
                w.hand = players_[idx].best_hand(community_);
                w.won_amount = per_player;
                w.description = players_[idx].best_hand(community_).to_string();
                results.push_back(w);
            } else {
                it->won_amount += per_player;
            }
        }
        prev_bet = bet;
    }

    return results;
}

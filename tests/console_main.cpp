#include "core/game_engine.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <random>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

static std::mt19937 rng(std::random_device{}());

// ── 中文字符串 ──

const char* phase_cn(Phase p) {
    switch (p) {
        case Phase::PreFlop: return "翻牌前";
        case Phase::Flop:    return "翻牌";
        case Phase::Turn:    return "转牌";
        case Phase::River:   return "河牌";
        case Phase::Showdown:return "摊牌";
    }
    return "???";
}

const char* hand_cn(const HandRank& h) {
    switch (h.type) {
        case HandType::HighCard:      return "高牌";
        case HandType::OnePair:       return "一对";
        case HandType::TwoPair:       return "两对";
        case HandType::ThreeOfAKind:  return "三条";
        case HandType::Straight:      return "顺子";
        case HandType::Flush:         return "同花";
        case HandType::FullHouse:     return "葫芦";
        case HandType::FourOfAKind:   return "四条";
        case HandType::StraightFlush: return "同花顺";
    }
    return "???";
}

std::string card_str(const Card& c) {
    const char* r[] = {"2","3","4","5","6","7","8","9","10","J","Q","K","A"};
    const char* s[] = {"\xE2\x99\xA3","\xE2\x99\xA6","\xE2\x99\xA5","\xE2\x99\xA0"};
    return std::string(r[static_cast<uint8_t>(c.rank)]) + s[static_cast<uint8_t>(c.suit)];
}

// ── 辅助输入 ──

int read_int(const std::string& prompt, int lo, int hi) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) return lo;
        try { int v = std::stoi(line); if (v >= lo && v <= hi) return v; }
        catch (...) {}
        std::cout << "  范围 " << lo << "-" << hi << "\n";
    }
}

bool read_bool(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) return false;
        if (line == "y" || line == "Y") return true;
        if (line == "n" || line == "N") return false;
        std::cout << "  输入 y/n\n";
    }
}

void press_enter() {
    std::cout << "  按回车继续...";
    std::string dummy; std::getline(std::cin, dummy);
}

// ── 手牌胜率估算 ──
// 翻牌前: 根据两张牌 rank/gap/suited 估算
// 翻牌后: 用牌型估算 + outs 调整

int preflop_equity(const Card& a, const Card& b) {
    int r1 = static_cast<int>(a.rank), r2 = static_cast<int>(b.rank);
    bool suited = a.suit == b.suit;
    bool pair = r1 == r2;
    int hi = std::max(r1, r2), lo = std::min(r1, r2);
    int gap = hi - lo - 1;

    if (pair) return 60 + (hi - 5) * 3;
    if (suited && gap == 0) return 52 + (hi - 8) * 2;
    if (suited && gap == 1) return 46 + (hi - 7) * 2;
    if (suited) return 38 + (hi - 8) * 2;
    if (gap == 0) return 42 + (hi - 8) * 2;
    if (hi >= 12 && lo >= 10) return 50;
    if (hi >= 12) return 40;
    if (hi >= 9)  return 35;
    return 28 + (hi - 5);
}

// 翻后: 合并手牌+公牌评估当前牌型，估算胜率
int postflop_equity(const Player& p, const std::vector<Card>& community) {
    if (p.hole_cards.size() < 2 || community.empty()) return 50;
    std::vector<Card> all = p.hole_cards;
    all.insert(all.end(), community.begin(), community.end());
    auto hand = HandEvaluator::evaluate(all);
    switch (hand.type) {
        case HandType::HighCard:      return 12 + hand.ranks[0] * 2;
        case HandType::OnePair:       return 30 + hand.ranks[0] * 2;
        case HandType::TwoPair:       return 55 + hand.ranks[0];
        case HandType::ThreeOfAKind:  return 72 + hand.ranks[0];
        case HandType::Straight:      return hand.ranks[0] >= 12 ? 92 : 85;
        case HandType::Flush:         return hand.ranks[0] >= 12 ? 90 : 82;
        case HandType::FullHouse:     return 95 + hand.ranks[0] / 4;
        case HandType::FourOfAKind:   return 98;
        case HandType::StraightFlush: return 99;
    }
    return 50;
}

// 计算补牌数(outs): 简化版，只统计同花/顺子补牌
int count_outs(const Player& p, const std::vector<Card>& community) {
    if (p.hole_cards.size() < 2) return 0;
    std::vector<Card> all = p.hole_cards;
    all.insert(all.end(), community.begin(), community.end());
    auto hand = HandEvaluator::evaluate(all);

    // 已有强牌则 outs=0 (不需要改进)
    if (hand.type >= HandType::Straight) return 0;
    if (hand.type >= HandType::ThreeOfAKind) return 2;  // 改进到葫芦/四条

    int outs = 0;
    // 同花听牌: 同花色已有 4 张
    int suit_cnt[4] = {0};
    for (auto& c : all) suit_cnt[static_cast<int>(c.suit)]++;
    for (int s = 0; s < 4; s++)
        if (suit_cnt[s] == 4) outs += 9;  // 9 张同花补牌

    // 顺子听牌: 简化
    uint8_t ranks[7];
    for (size_t i = 0; i < all.size(); i++)
        ranks[i] = static_cast<uint8_t>(all[i].rank);
    std::sort(ranks, ranks + all.size());
    // 两端顺子听牌约有 8 outs，卡顺有 4 outs
    uint8_t* end = std::unique(ranks, ranks + all.size());
    int unique = (int)(end - ranks);
    if (unique >= 4) {
        for (int i = 0; i <= unique - 4; i++) {
            int gap = (ranks[i+3] - ranks[i]) - 3;
            if (gap <= 2) outs += (3 - gap) * 4;  // open-ended: 8, gutshot: 4
        }
    }

    // 对子改进到三条: 2 outs
    if (hand.type == HandType::OnePair) outs += 2;
    // 两对改进到葫芦: 4 outs
    if (hand.type == HandType::TwoPair) outs += 4;

    return std::min(outs, 20);
}

// 综合胜率估算
int estimate_equity(const Player& p, const std::vector<Card>& community) {
    if (community.empty())
        return preflop_equity(p.hole_cards[0], p.hole_cards[1]);

    int base = postflop_equity(p, community);
    int outs = count_outs(p, community);
    int remaining = 52 - 2 - (int)community.size();
    if (remaining > 0)
        base += (outs * 100 / remaining) / 2; // 补牌提升

    return std::min(base, 99);
}

// ── 位置计算 ──
// 返回 0=BTN(最晚) ~ n-2=UTG(最早), n-1=盲注
int position_index(const GameEngine& game, int player_idx) {
    int n = (int)game.players().size();
    int dealer = game.dealer_index();
    if (dealer < 0 || dealer >= n) return n;
    // dealer 右手第一个 = 位置 0 (UTG 在 4 人局)
    int pos = 0;
    for (int i = (dealer + 1) % n; i != dealer; i = (i + 1) % n, pos++) {
        if (i == player_idx) {
            // BTN=dealer 是最晚位置
            if (player_idx == dealer && game.phase() != Phase::PreFlop) return n - 1;
            if (pos >= n - 2) return n - 1; // 盲注位置最后
            return pos;
        }
    }
    return 0;
}

std::string hand_desc(const Card& a, const Card& b) {
    int r1 = static_cast<int>(a.rank), r2 = static_cast<int>(b.rank);
    const char* ranks[] = {"2","3","4","5","6","7","8","9","10","J","Q","K","A"};
    bool suited = a.suit == b.suit;
    if (r1 == r2) return std::string("对") + ranks[r1];
    if (suited) return std::string(ranks[std::max(r1,r2)]) + ranks[std::min(r1,r2)] + "s";
    return std::string(ranks[std::max(r1,r2)]) + ranks[std::min(r1,r2)] + "o";
}

// ── 显示状态 ──

void print_state(GameEngine& game) {
    std::cout << "\n┌──────────────────────────────────────┐\n";
    std::cout << "│ 阶段: " << phase_cn(game.phase())
              << "  │ 底池: $" << game.pot()
              << "  │ 当前注: $" << game.current_bet()
              << "   │\n";
    std::cout << "└──────────────────────────────────────┘\n";

    auto& comm = game.community();
    if (!comm.empty()) {
        std::cout << "  公牌: ";
        for (size_t i = 0; i < comm.size(); i++) {
            std::cout << card_str(comm[i]);
            if (i < comm.size() - 1) std::cout << "  ";
            if (i == 2) std::cout << "  ";
        }
        for (size_t i = comm.size(); i < 5; i++) std::cout << " ? ";
        std::cout << "\n";
    }

    int cur = game.current_player();
    for (int i = 0; i < (int)game.players().size(); ++i) {
        auto& p = game.players()[i];
        std::cout << (i == cur ? " →" : "  ")
                  << "座" << i << " " << p.name
                  << "  $" << p.chips
                  << "  注$" << p.bet_amount;
        if (p.folded) std::cout << " [弃]";
        if (p.is_all_in) std::cout << " [全下]";
        std::cout << "\n";
    }

    if (!game.is_hand_over()) {
        auto acts = game.allowed_actions(cur);
        std::cout << "  操作: ";
        for (auto a : acts) {
            switch (a) {
                case ActionType::Fold:  std::cout << "[f]弃 "; break;
                case ActionType::Check: std::cout << "[k]过 "; break;
                case ActionType::Call:  std::cout << "[c]跟 "; break;
                case ActionType::Raise: std::cout << "[r]加 "; break;
                case ActionType::AllIn: std::cout << "[a]全下 "; break;
            }
        }
        std::cout << "\n";
    }
}

void show_hand(GameEngine& game) {
    auto& p = game.players()[game.current_player()];
    std::cout << "\n  手牌: ";
    if (p.hole_cards.size() == 2) {
        int eq = estimate_equity(p, game.community());
        std::cout << card_str(p.hole_cards[0]) << " " << card_str(p.hole_cards[1])
                  << " [" << hand_desc(p.hole_cards[0], p.hole_cards[1])
                  << " 胜率≈" << eq << "%]\n";
    }
    std::cout << "  输入> ";
}

// ── 机器人 ──

static std::vector<int> ai_levels; // 每人一个难度

void bot_play(GameEngine& game, bool verbose = true) {
    int idx = game.current_player();
    int lv = (idx < (int)ai_levels.size()) ? ai_levels[idx] : 2;
    auto allowed = game.allowed_actions(idx);
    auto& p = game.players()[idx];
    if (allowed.empty()) return;

    bool can_check = false, can_call = false, can_raise = false;
    int call_amt = game.current_bet() - p.bet_amount;
    for (auto a : allowed) {
        if (a == ActionType::Check) can_check = true;
        if (a == ActionType::Call)  can_call = true;
        if (a == ActionType::Raise) can_raise = true;
    }

    // 手牌强度
    int equity = estimate_equity(p, game.community());

    // 底池赔率
    int pot = game.pot();
    int pot_odds_pct = 0;
    if (call_amt > 0)
        pot_odds_pct = (int)(100.0 * call_amt / (pot + call_amt + call_amt));

    int r = std::uniform_int_distribution<>(0, 99)(rng);

    Action act; act.type = ActionType::Fold;
    std::string reason;

    // LV 参数: {fold强牌阈值, raise率, bluff率}
    // ── 难度参数 + 位置调整 ──
    int base_fold = 0, base_raise = 0, base_bluff = 0;
    if (lv == 1)      { base_fold = 30; base_raise =  5; base_bluff =  0; }
    else if (lv == 2) { base_fold = 18; base_raise = 15; base_bluff =  3; }
    else              { base_fold =  8; base_raise = 28; base_bluff =  8; }

    int pos = position_index(game, idx);
    int n = (int)game.players().size();
    double pos_adj = (pos > n / 2) ? 1.15 : (pos < n / 3) ? 0.85 : 1.0;
    int fold_thresh = (int)(base_fold * (2.0 - pos_adj));
    int raise_rate  = (int)(base_raise * pos_adj);
    int bluff_rate  = (int)(base_bluff * pos_adj);

    // ── 下注尺度 ──
    int total_chips = p.chips + p.bet_amount;
    int min_r = game.current_bet() + game.min_raise();
    int small_bet = std::min(min_r * 2, total_chips);
    int med_bet   = std::min(pot / 2 + game.current_bet(), total_chips);
    int large_bet = std::min(pot + game.current_bet(), total_chips);

    // ── 决策 ──
    if (can_check && can_raise) {
        if (r < raise_rate && equity > 55) {
            act.type = ActionType::Raise;
            act.amount = equity > 75 ? med_bet : small_bet;
            reason = "价值加注";
        } else if (r < raise_rate / 2) {
            act.type = ActionType::Raise;
            act.amount = small_bet;
            reason = "试探下注";
        } else {
            act.type = ActionType::Check;
            reason = equity > 50 ? "慢打过牌" : "控池过牌";
        }
    } else if (can_check) {
        act.type = ActionType::Check;
        reason = "过牌";
    } else if (can_call) {
        if (can_raise && r < raise_rate && equity > 55) {
            act.type = ActionType::Raise;
            if (equity > 80) act.amount = large_bet;
            else if (equity > 65) act.amount = med_bet;
            else act.amount = small_bet;
            reason = "价值加注(" + std::to_string(equity) + "%)";
        } else if (r < bluff_rate && can_raise) {
            act.type = ActionType::Raise;
            act.amount = large_bet;
            reason = "诈唬!";
        } else if (equity < fold_thresh && r < 70) {
            act.type = ActionType::Fold;
            reason = "牌力不足(" + std::to_string(equity) + "%<" + std::to_string(fold_thresh) + "%)";
        } else if (equity < pot_odds_pct && r < 75) {
            act.type = ActionType::Fold;
            reason = "赔率差(" + std::to_string(equity) + "%<" + std::to_string(pot_odds_pct) + "%)";
        } else {
            act.type = ActionType::Call;
            reason = equity > pot_odds_pct
                ? "+EV跟(胜率" + std::to_string(equity) + "%>赔率" + std::to_string(pot_odds_pct) + "%)"
                : "跟注看牌";
        }
    }

    if (verbose) {
        const char* pos_str[] = {"UTG","EP","MP","HJ","CO","BTN","SB","BB","DEF"};
        int pi = std::min(pos, 8);
        std::cout << "    " << p.name << " [LV" << lv << " " << pos_str[pi] << "] ";
        if (p.hole_cards.size() == 2)
            std::cout << "{" << card_str(p.hole_cards[0]) << card_str(p.hole_cards[1])
                      << " 胜率≈" << equity << "%} ";

        switch (act.type) {
            case ActionType::Fold:
                std::cout << "弃牌"; break;
            case ActionType::Check:
                std::cout << "过牌"; break;
            case ActionType::Call:
                std::cout << "跟注 $" << call_amt; break;
            case ActionType::Raise:
                std::cout << "加注到 $" << act.amount; break;
            case ActionType::AllIn:
                std::cout << "全下!"; break;
        }
        if (!reason.empty()) std::cout << "  ←" << reason;
        std::cout << "\n";
    }

    auto result = game.player_action(idx, act);
    if (result == ActionResult::Invalid && can_call)
        game.player_action(idx, Action{ActionType::Call, 0});
    else if (result == ActionResult::Invalid)
        game.player_action(idx, Action{ActionType::Fold, 0});
}

void auto_play(GameEngine& game, int human_seat, bool verbose) {
    for (int s = 0; !game.is_hand_over() && game.current_player() != human_seat && s < 500; s++)
        bot_play(game, verbose);
}

// ── 每局 ──

void show_results(GameEngine& game);

bool play_human(GameEngine& game, int human_seat) {
    game.start_hand();
    while (!game.is_hand_over()) {
        auto_play(game, human_seat, true);
        if (game.is_hand_over()) break;
        print_state(game);
        show_hand(game);

        std::string line;
        if (!std::getline(std::cin, line)) return false;
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string cmd; int amt = 0;
        iss >> cmd >> amt;

        Action act;
        if (cmd == "f") act.type = ActionType::Fold;
        else if (cmd == "c") act.type = ActionType::Call;
        else if (cmd == "k") act.type = ActionType::Check;
        else if (cmd == "r") { act.type = ActionType::Raise; act.amount = amt; }
        else if (cmd == "a") { act.type = ActionType::AllIn; }
        else { std::cout << "  无效!\n"; continue; }

        auto r = game.player_action(game.current_player(), act);
        if (r == ActionResult::Invalid) std::cout << "  无效操作!\n";
    }
    show_results(game);
    return true;
}

void play_spectate(GameEngine& game) {
    game.start_hand();
    while (!game.is_hand_over()) {
        print_state(game);
        // 显示当前玩家的手牌（观战可以看到所有人的牌）
        int cur = game.current_player();
        auto& p = game.players()[cur];
        if (p.hole_cards.size() == 2) {
            int equity = estimate_equity(p, game.community());
            std::cout << "  [" << p.name << "手牌: "
                      << card_str(p.hole_cards[0]) << card_str(p.hole_cards[1])
                      << " " << hand_desc(p.hole_cards[0], p.hole_cards[1])
                      << " 胜率≈" << equity << "%]\n";
        }
        auto_play(game, -1, true);
        press_enter();
    }
    show_results(game);
}

void show_results(GameEngine& game) {
    auto winners = game.get_winners();
    auto& comm = game.community();
    std::cout << "\n╔══════════════════════════════════╗\n";
    std::cout << "║            摊牌结果              ║\n";
    std::cout << "╚══════════════════════════════════╝\n";
    std::cout << "\n  公牌: ";
    for (auto& c : comm) std::cout << card_str(c) << " ";
    std::cout << "\n\n";
    for (int i = 0; i < (int)game.players().size(); ++i) {
        auto& p = game.players()[i];
        std::cout << "  " << p.name << ": ";
        if (p.hole_cards.size() >= 2)
            std::cout << card_str(p.hole_cards[0]) << " " << card_str(p.hole_cards[1]);
        if (!p.folded) {
            auto h = p.best_hand(comm);
            std::cout << " → " << hand_cn(h);
        } else std::cout << " (弃)";
        std::cout << "\n";
    }
    for (auto& w : winners) {
        std::cout << "\n  ★ " << game.players()[w.player_index].name
                  << " 赢得 $" << w.won_amount << " (" << hand_cn(w.hand) << ")\n";
    }
}

// ── 设置 ──

void setup_blinds(int& sb, int& bb) {
    std::cout << "\n  盲注等级:\n";
    std::cout << "    1. 微额   $1/$2\n";
    std::cout << "    2. 低额   $5/$10\n";
    std::cout << "    3. 中额   $25/$50\n";
    std::cout << "    4. 高额   $100/$200\n";
    std::cout << "    5. 自定义\n";
    int c = read_int("  选择: ", 1, 5);
    if (c == 1)      { sb = 1; bb = 2; }
    else if (c == 2) { sb = 5; bb = 10; }
    else if (c == 3) { sb = 25; bb = 50; }
    else if (c == 4) { sb = 100; bb = 200; }
    else if (c == 5) {
        sb = read_int("  小盲: ", 1, 100000);
        bb = read_int("  大盲: ", sb + 1, 1000000);
    }
    std::cout << "  盲注: $" << sb << "/$" << bb << "\n";
}

int setup_players_count(int& human_seat) {
    ai_levels.clear();

    int n = read_int("  人数 (2-9): ", 2, 9);
    std::cout << "\n  模式: 1.人机对战  2.观战\n";
    int mode = read_int("  选择: ", 1, 2);
    human_seat = (mode == 1) ? read_int("  你的座位 (0-" + std::to_string(n-1) + "): ", 0, n-1) : -1;

    std::cout << "\n  AI 难度: 1.简单(鱼) 2.普通 3.困难(紧凶)\n";
    for (int i = 0; i < n; i++) {
        if (human_seat >= 0 && i == human_seat)
            ai_levels.push_back(2);
        else
            ai_levels.push_back(read_int("  座位" + std::to_string(i) + " 难度: ", 1, 3));
    }
    return n;
}

void give_chips(GameEngine& game, int human_seat, int n) {
    std::cout << "\n  筹码:\n";
    std::cout << "    1. 统一 $500\n    2. 统一 $1000\n    3. 统一 $10000\n";
    std::cout << "    4. 统一自定义\n    5. 每人单独设置\n";
    int c = read_int("  选择: ", 1, 5);

    const char* ai_names[] = {
        "\xE2\x99\xA0" "黑桃","\xE2\x99\xA5" "红心",
        "\xE2\x99\xA6" "方块","\xE2\x99\xA3" "梅花",
        "小盲","大盲","庄家","枪口","关煞"
    };

    // 先加名字，筹码后面赋值
    for (int i = 0; i < n; i++) {
        std::string name;
        if (human_seat >= 0 && i == human_seat)
            name = "你";
        else
            name = ai_names[i % 9];
        game.add_player(name, 0);
    }

    if (c >= 1 && c <= 4) {
        int chips = 1000;
        if (c == 1) chips = 500;
        else if (c == 3) chips = 10000;
        else if (c == 4) chips = read_int("  统一金额 (1-1000000000): ", 1, 1000000000);
        for (int i = 0; i < n; i++)
            const_cast<std::vector<Player>&>(game.players())[i].chips = chips;
    } else {
        for (int i = 0; i < n; i++) {
            std::string label = game.players()[i].name;
            int chips = read_int("  " + label + " 筹码 (1-1000000000): ", 1, 1000000000);
            const_cast<std::vector<Player>&>(game.players())[i].chips = chips;
        }
    }

    std::cout << "\n  当前筹码:\n";
    for (int i = 0; i < n; i++) {
        auto& p = const_cast<std::vector<Player>&>(game.players())[i];
        std::cout << "    " << p.name << ": $" << p.chips << "\n";
    }
}

void handle_busted_players(GameEngine& game, int human_seat) {
    auto& pl = const_cast<std::vector<Player>&>(game.players());
    for (int i = (int)pl.size() - 1; i >= 0; i--) {
        if (pl[i].chips <= 0) {
            bool is_human = (i == human_seat);
            std::cout << "\n  " << pl[i].name << " 筹码归零!\n";
            if (is_human || human_seat < 0) {
                // 人类或观战模式: 问是否补筹码
                std::cout << "    1. 补充筹码继续\n    2. 淘汰离开\n";
                int choice = read_int("  选择: ", 1, 2);
                if (choice == 1) {
                    int rebuy = read_int("  补充金额: ", 1, 1000000000);
                    pl[i].chips = rebuy;
                    std::cout << "  " << pl[i].name << " 补充 $" << rebuy << "\n";
                    continue;
                }
            }
            // AI 或选择淘汰
            std::cout << "  " << pl[i].name << " 被淘汰\n";
            pl.erase(pl.begin() + i);
            if (i <= human_seat && human_seat >= 0) human_seat--;
            // 更新 AI 难度数组
            if (i < (int)ai_levels.size()) ai_levels.erase(ai_levels.begin() + i);
        }
    }
}

bool game_loop(GameEngine& game, int human_seat) {
    int hand = 1;
    while (true) {
        if (game.players().size() < 2) {
            std::cout << "\n  对手不足!\n"; return false;
        }
        std::cout << "\n══════════ 第 " << hand << " 局 ══════════\n";
        if (human_seat >= 0) {
            if (!play_human(game, human_seat)) return false;
        } else {
            play_spectate(game);
        }
        hand++;

        // 处理破产玩家
        handle_busted_players(game, human_seat);
        if (game.players().size() < 2) {
            std::cout << "\n  玩家不足, 游戏结束!\n";
            return false;
        }

        if (!read_bool("\n  继续下一局? (y/n): ")) return false;
    }
}

// ── 主菜单 ──

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    while (true) {
        std::cout << "\n"
            "╔══════════════════════════════╗\n"
            "║   \xE2\x99\xA0 德 州 扑 克 \xE2\x99\xA5          ║\n"
            "║     Texas Hold'em          ║\n"
            "╚══════════════════════════════╝\n";
        std::cout << "  1. 开始游戏\n  2. 退出\n";
        if (read_int("  选择: ", 1, 2) == 2) break;

        GameEngine game(1, 2);
        int human_seat = 0;
        int n = setup_players_count(human_seat);
        int sb = 0, bb = 0;
        setup_blinds(sb, bb);

        // Recreate game with correct blinds
        game = GameEngine(sb, bb);
        give_chips(game, human_seat, n);
        game_loop(game, human_seat);
    }
    std::cout << "\n  再见!\n";
    return 0;
}

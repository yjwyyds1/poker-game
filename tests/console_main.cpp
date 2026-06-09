#include "core/game_engine.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <random>
#include <limits>

#ifdef _WIN32
#include <windows.h>
#endif

static std::mt19937 rng(std::random_device{}());

// ── 中文字符串映射 ──

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

std::string card_cn_str(const Card& c) {
    const char* ranks[] = {"2","3","4","5","6","7","8","9","10","J","Q","K","A"};
    const char* suits[] = {"\xE2\x99\xA3","\xE2\x99\xA6","\xE2\x99\xA5","\xE2\x99\xA0"};
    return std::string(ranks[static_cast<uint8_t>(c.rank)])
         + suits[static_cast<uint8_t>(c.suit)];
}

// ── 辅助输入函数 ──

int read_int(const std::string& prompt, int min_val, int max_val) {
    while (true) {
        std::cout << prompt;
        std::string line;
        std::getline(std::cin, line);
        try {
            int v = std::stoi(line);
            if (v >= min_val && v <= max_val) return v;
        } catch (...) {}
        std::cout << "  输入无效，请输入 " << min_val << "-" << max_val << "\n";
    }
}

bool read_bool(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        std::string line;
        std::getline(std::cin, line);
        if (line == "y" || line == "Y") return true;
        if (line == "n" || line == "N") return false;
        std::cout << "  输入 y 或 n\n";
    }
}

// ── 显示状态 ──

void print_state(GameEngine& game) {
    std::cout << "\n┌──────────────────────────────────────┐\n";
    std::cout << "│ 阶段: " << phase_cn(game.phase())
              << "  │ 底池: $" << game.pot()
              << "  │ 当前下注: $" << game.current_bet()
              << "   │\n";
    std::cout << "└──────────────────────────────────────┘\n";

    auto& comm = game.community();
    if (!comm.empty()) {
        std::cout << "  公牌: ";
        for (size_t i = 0; i < comm.size(); i++) {
            std::cout << card_cn_str(comm[i]);
            if (i < comm.size() - 1) std::cout << "  ";
            if (i == 2) std::cout << "  ";
        }
        for (size_t i = comm.size(); i < 5; i++) std::cout << " ? ";
        std::cout << "\n";
    }

    int cur = game.current_player();
    for (int i = 0; i < static_cast<int>(game.players().size()); ++i) {
        auto& p = game.players()[i];
        std::cout << (i == cur ? " →" : "  ")
                  << "座位" << i << " " << p.name
                  << "  筹码:$" << p.chips
                  << "  下注:$" << p.bet_amount;
        if (p.folded) std::cout << " [弃牌]";
        if (p.is_all_in) std::cout << " [全下]";
        std::cout << "\n";
    }

    if (!game.is_hand_over()) {
        auto actions = game.allowed_actions(cur);
        std::cout << "  可操作: ";
        for (auto a : actions) {
            switch (a) {
                case ActionType::Fold:  std::cout << "[f]弃牌 "; break;
                case ActionType::Check: std::cout << "[k]过牌 "; break;
                case ActionType::Call:  std::cout << "[c]跟注 "; break;
                case ActionType::Raise: std::cout << "[r]加注 "; break;
                case ActionType::AllIn: std::cout << "[a]全下 "; break;
            }
        }
        std::cout << "\n";
    }
}

void show_hand_cards(GameEngine& game) {
    int cur = game.current_player();
    auto& p = game.players()[cur];
    std::cout << "\n  你的手牌: ";
    if (p.hole_cards.size() >= 2) {
        std::cout << card_cn_str(p.hole_cards[0]) << "  "
                  << card_cn_str(p.hole_cards[1]);
    }
    std::cout << "\n  操作> ";
}

// ── 机器人逻辑 ──

static int ai_level = 2; // 1=简单 2=普通 3=困难

void bot_play(GameEngine& game) {
    int idx = game.current_player();
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

    Action act; act.type = ActionType::Fold;
    int r = std::uniform_int_distribution<>(0, 99)(rng);

    // 根据 AI 等级调整决策
    int fold_rate   = (ai_level == 1) ? 30 : (ai_level == 2) ? 15 : 5;
    int raise_rate  = (ai_level == 1) ?  5 : (ai_level == 2) ? 15 : 30;
    int bluff_rate  = (ai_level == 1) ?  0 : (ai_level == 3) ? 10 : 0;

    if (can_check && can_raise) {
        if (r < raise_rate) {
            act.type = ActionType::Raise;
            act.amount = game.current_bet() + game.min_raise();
        } else {
            act.type = ActionType::Check;
        }
    } else if (can_check) {
        act.type = ActionType::Check;
    } else if (can_call) {
        float ratio = call_amt > 0 ? (float)call_amt / (float)(game.pot() + call_amt) : 0;
        if (can_raise && r < raise_rate) {
            act.type = ActionType::Raise;
            act.amount = game.current_bet() + game.min_raise();
        } else if (r < fold_rate && ratio > 0.5f && can_raise) {
            // 偶尔 bluff raise
            act.type = ActionType::Raise;
            act.amount = game.current_bet() + game.min_raise() * 2;
        } else if (r < fold_rate + 5) {
            act.type = ActionType::Fold;
        } else {
            act.type = ActionType::Call;
        }
    }

    // 打印机器人操作
    std::cout << "    " << p.name << ": ";
    switch (act.type) {
        case ActionType::Fold:  std::cout << "弃牌\n"; break;
        case ActionType::Check: std::cout << "过牌\n"; break;
        case ActionType::Call:  std::cout << "跟注 $" << call_amt << "\n"; break;
        case ActionType::Raise: std::cout << "加注到 $" << act.amount << "\n"; break;
        case ActionType::AllIn: std::cout << "全下!\n"; break;
    }

    auto result = game.player_action(idx, act);
    if (result == ActionResult::Invalid) {
        game.player_action(idx, Action{ActionType::Fold, 0});
    }
}

void auto_play_bots(GameEngine& game, int human_seat) {
    for (int s = 0; !game.is_hand_over() && game.current_player() != human_seat && s < 500; s++)
        bot_play(game);
}

// ── 单局游戏（人机对战）──

void show_results(GameEngine& game); // forward

bool play_one_hand(GameEngine& game, int human_seat) {
    game.start_hand();

    while (!game.is_hand_over()) {
        auto_play_bots(game, human_seat);
        if (game.is_hand_over()) break;

        print_state(game);
        show_hand_cards(game);

        std::string line;
        if (!std::getline(std::cin, line)) return false;
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd;
        int amount = 0;
        iss >> cmd >> amount;

        Action action;
        if (cmd == "f") action.type = ActionType::Fold;
        else if (cmd == "c") action.type = ActionType::Call;
        else if (cmd == "k") action.type = ActionType::Check;
        else if (cmd == "r") { action.type = ActionType::Raise; action.amount = amount; }
        else if (cmd == "a") { action.type = ActionType::AllIn; }
        else { std::cout << "  无效指令!\n"; continue; }

        auto result = game.player_action(game.current_player(), action);
        if (result == ActionResult::Invalid)
            std::cout << "  无效操作! 请重试\n";
    }

    show_results(game);
    return true;
}

// ── 单局观战（全部 AI）──

void play_spectate(GameEngine& game) {
    game.start_hand();
    while (!game.is_hand_over()) {
        print_state(game);
        auto_play_bots(game, -1); // 没有人类，全部机器人
        std::cout << "\n  按回车继续观战...";
        std::string dummy;
        std::getline(std::cin, dummy);
    }
    show_results(game);
}

// ── 摊牌结果 ──

void show_results(GameEngine& game) {
    auto winners = game.get_winners();
    auto& comm = game.community();

    std::cout << "\n╔══════════════════════════════════╗\n";
    std::cout << "║           摊  牌  结  果          ║\n";
    std::cout << "╚══════════════════════════════════╝\n";
    std::cout << "\n  公牌: ";
    for (auto& c : comm) std::cout << card_cn_str(c) << " ";
    std::cout << "\n\n";

    for (int i = 0; i < static_cast<int>(game.players().size()); ++i) {
        auto& p = game.players()[i];
        std::cout << "  " << p.name << ": ";
        if (p.hole_cards.size() >= 2)
            std::cout << card_cn_str(p.hole_cards[0]) << " " << card_cn_str(p.hole_cards[1]);
        if (!p.folded) {
            auto hand = p.best_hand(comm);
            std::cout << " → " << hand_cn(hand);
        } else {
            std::cout << " (弃牌)";
        }
        std::cout << "\n";
    }

    for (auto& w : winners) {
        std::cout << "\n  ★ " << game.players()[w.player_index].name
                  << " 赢得 $" << w.won_amount << " (" << hand_cn(w.hand) << ")\n";
    }

    // 淘汰无筹码玩家
    auto& pls = const_cast<std::vector<Player>&>(game.players());
    pls.erase(std::remove_if(pls.begin(), pls.end(),
        [](const Player& p) { return p.chips <= 0; }), pls.end());
}

// ── 主菜单 ──

void show_title() {
    std::cout << R"(
╔══════════════════════════════════════╗
║          ♠ 德 州 扑 克 ♥            ║
║           Texas Hold'em             ║
╚══════════════════════════════════════╝
)";
}

int setup_game(GameEngine& game, int& human_seat) {
    // 清空旧玩家
    auto& pls = const_cast<std::vector<Player>&>(game.players());
    pls.clear();

    int num_players = read_int("  游戏人数 (2-9): ", 2, 9);

    std::cout << "\n  选择模式:\n";
    std::cout << "    1. 人机对战（你 vs AI）\n";
    std::cout << "    2. 观战模式（AI vs AI）\n";
    int mode = read_int("  选择: ", 1, 2);

    if (mode == 1) {
        human_seat = read_int("  选择你的座位 (0-" + std::to_string(num_players - 1) + "): ",
                              0, num_players - 1);
    } else {
        human_seat = -1;
    }

    std::cout << "\n  AI 难度:\n";
    std::cout << "    1. 简单（鱼）\n";
    std::cout << "    2. 普通（娱乐玩家）\n";
    std::cout << "    3. 困难（紧凶型）\n";
    ai_level = read_int("  选择: ", 1, 3);

    return num_players;
}

void give_chips(GameEngine& game, int human_seat, int num_players) {
    std::cout << "\n  ── 筹码设置 ──\n";
    std::cout << "    1. 领低保 $500\n";
    std::cout << "    2. 标准场 $1000\n";
    std::cout << "    3. 自定义金额\n";
    int choice = read_int("  选择: ", 1, 3);

    int human_chips = 1000;
    if (choice == 1) human_chips = 500;
    else if (choice == 3) human_chips = read_int("  输入金额: ", 1, 999999);

    int bot_chips = human_chips;
    if (human_seat >= 0) {
        std::cout << "  机器人筹码与玩家相同 ($" << bot_chips << ")\n";
    }

    for (int i = 0; i < num_players; i++) {
        std::string name;
        if (human_seat >= 0 && i == human_seat) {
            name = "你";
        } else {
            const char* ai_names[] = {"Alpha","Bravo","Charlie","Delta","Echo",
                                      "Fox","Golf","Hotel","India"};
            name = ai_names[i % 9];
        }
        game.add_player(name, i == human_seat ? human_chips : bot_chips);
    }
}

bool game_loop(GameEngine& game, int human_seat) {
    int hand_no = 1;
    while (true) {
        if (game.players().size() < 2) {
            std::cout << "\n  对手不足，游戏结束!\n";
            return false;
        }

        std::cout << "\n══════════ 第 " << hand_no << " 局 ══════════\n";

        if (human_seat >= 0) {
            if (!play_one_hand(game, human_seat)) return false;
        } else {
            play_spectate(game);
        }

        hand_no++;

        if (!read_bool("\n  继续下一局? (y/n): ")) return false;
    }
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    while (true) {
        show_title();

        std::cout << "  1. 开始游戏\n";
        std::cout << "  2. 退出\n";
        int menu = read_int("  选择: ", 1, 2);
        if (menu == 2) break;

        GameEngine game(1, 2);
        int human_seat = 0;
        int num = setup_game(game, human_seat);
        give_chips(game, human_seat, num);

        game_loop(game, human_seat);
    }

    std::cout << "\n  再见!\n";
    return 0;
}

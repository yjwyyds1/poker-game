#include "core/game_engine.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <random>

static std::mt19937 rng(std::random_device{}());

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
    const char* suits[] = {"♣","♦","♥","♠"};
    return std::string(ranks[static_cast<uint8_t>(c.rank)])
         + suits[static_cast<uint8_t>(c.suit)];
}

void print_state(GameEngine& game) {
    std::cout << "\n┌──────────────────────────────────┐\n";
    std::cout << "│ 阶段: " << phase_cn(game.phase())
              << "  │ 底池: $" << game.pot()
              << "  │ 当前下注: $" << game.current_bet()
              << "   │\n";
    std::cout << "└──────────────────────────────────┘\n";

    // 公牌
    auto& comm = game.community();
    if (!comm.empty()) {
        std::cout << "  公牌: ";
        for (size_t i = 0; i < comm.size(); i++) {
            std::cout << card_cn_str(comm[i]);
            if (i < comm.size() - 1) std::cout << "  ";
            if (i == 2) std::cout << "  "; // 翻牌后加空位
        }
        if (comm.size() < 5) {
            for (size_t i = comm.size(); i < 5; i++) std::cout << " ? ";
        }
        std::cout << "\n";
    }

    // 玩家
    int cur = game.current_player();
    for (int i = 0; i < static_cast<int>(game.players().size()); ++i) {
        auto& p = game.players()[i];
        std::cout << (i == cur ? " →" : "  ")
                  << "玩家" << i << " " << p.name
                  << "  筹码:$" << p.chips
                  << "  下注:$" << p.bet_amount;
        if (p.folded) std::cout << " [弃牌]";
        if (p.is_all_in) std::cout << " [全下]";
        std::cout << "\n";
    }

    // 允许的操作
    if (!game.is_hand_over()) {
        auto actions = game.allowed_actions(game.current_player());
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

void bot_play(GameEngine& game) {
    int idx = game.current_player();
    auto allowed = game.allowed_actions(idx);
    if (allowed.empty()) return;

    bool can_check = false, can_call = false;
    for (auto a : allowed) {
        if (a == ActionType::Check) can_check = true;
        if (a == ActionType::Call) can_call = true;
    }

    Action act; act.type = ActionType::Fold;
    if (can_check) act.type = ActionType::Check;
    else if (can_call && std::uniform_int_distribution<>(0, 99)(rng) < 85)
        act.type = ActionType::Call;

    game.player_action(idx, act);
}

void auto_play_bots(GameEngine& game) {
    for (int s = 0; !game.is_hand_over() && game.current_player() != 0 && s < 500; s++)
        bot_play(game);
}

bool play_one_hand(GameEngine& game) {
    game.start_hand();

    while (!game.is_hand_over()) {
        // 自动跑机器人回合
        auto_play_bots(game);
        if (game.is_hand_over()) break;

        // 轮到你了
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
        else {
            std::cout << "  无效指令!\n";
            continue;
        }

        auto result = game.player_action(game.current_player(), action);
        if (result == ActionResult::Invalid) {
            std::cout << "  无效操作! 请重试\n";
        }
    }

    // 摊牌
    auto winners = game.get_winners();
    auto& comm = game.community();

    std::cout << "\n╔══════════════════════════════════╗\n";
    std::cout << "║           摊  牌  结  果           ║\n";
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

    // 淘汰无筹码的玩家
    auto& pls = const_cast<std::vector<Player>&>(game.players());
    pls.erase(std::remove_if(pls.begin(), pls.end(),
        [](const Player& p) { return p.chips <= 0; }), pls.end());

    return true;
}

int main() {
    GameEngine game(1, 2);
    game.add_player("你", 100);
    game.add_player("机器人A", 100);
    game.add_player("机器人B", 100);

    int hand_no = 1;
    while (true) {
        if (game.players().size() < 2) {
            std::cout << "\n对手不足，游戏结束!\n";
            break;
        }

        std::cout << "\n══════════ 第 " << hand_no << " 局 ══════════\n";

        if (!play_one_hand(game)) break;

        hand_no++;

        std::cout << "\n继续下一局? (y=继续 n=退出): ";
        std::string ans;
        std::getline(std::cin, ans);
        if (ans == "n" || ans == "N") break;
    }

    std::cout << "\n游戏结束. 谢谢!\n";
    return 0;
}

#define _WIN32_WINNT 0x0A00
#include "lib/httplib.h"
#include "lib/json.hpp"
#include <iostream>
#include <string>

using json = nlohmann::json;

int main() {
    httplib::ws::WebSocketClient ws("ws://localhost:8080/ws");

    if (!ws.connect()) {
        std::cerr << "Failed to connect\n";
        return 1;
    }
    std::cout << "Connected!\n";

    std::string msg;
    int round = 0;

    while (round < 3 && ws.is_open()) {
        auto result = ws.read(msg);
        if (result != httplib::ws::ReadResult::Text) {
            std::cout << "Read result: " << (int)result << " (closed or error)\n";
            break;
        }

        try {
            auto state = json::parse(msg);
            bool hand_over = state.value("hand_over", false);

            if (hand_over) {
                std::cout << "\n=== HAND OVER ===\n";
                std::cout << "Pot: " << state["pot"] << "\n";
                std::cout << "Board: ";
                for (auto& c : state["community"])
                    std::cout << c["rank"] << c["suit"] << " ";
                std::cout << "\n";
                for (auto& w : state["winners"]) {
                    int pi = w["player"];
                    std::cout << state["players"][pi]["name"] << " wins " << w["amount"]
                              << " (" << w["hand"] << ")\n";
                }

                json new_hand = {{"type", "new_hand"}};
                ws.send(new_hand.dump());
                round++;
                std::cout << "\n--- New hand #" << round << " ---\n";
                continue;
            }

            int current = state["current_player"];
            int you = -1;
            for (auto& p : state["players"]) {
                if (p.value("is_you", false)) { you = p["index"]; break; }
            }

            if (current != you) {
                continue; // Bot turn, skip output
            }

            std::string phase = state["phase"];
            int pot = state["pot"];

            std::cout << "\nTurn [" << phase << "] pot=" << pot;
            auto you_p = state["players"][you];
            std::cout << " Cards: ";
            for (auto& c : you_p["hole_cards"])
                std::cout << c["rank"] << c["suit"] << " ";
            std::cout << "Board: ";
            for (auto& c : state["community"])
                std::cout << c["rank"] << c["suit"] << " ";
            std::cout << "Actions: ";
            for (auto& a : state["allowed_actions"]) std::cout << a.get<std::string>() << " ";

            bool has_check = false;
            for (auto& a : state["allowed_actions"]) {
                if (a.get<std::string>() == "check") has_check = true;
            }

            json action;
            if (has_check) {
                action = {{"action", "check"}, {"amount", 0}};
                std::cout << " -> check\n";
            } else {
                action = {{"action", "call"}, {"amount", 0}};
                std::cout << " -> call\n";
            }

            if (!ws.send(action.dump())) {
                std::cout << "Send failed!\n";
                break;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << " raw=" << msg.substr(0,100) << "\n";
        }
    }

    ws.close(httplib::ws::CloseStatus::Normal);
    std::cout << "\nDone.\n";
    return 0;
}

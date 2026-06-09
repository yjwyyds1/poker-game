# PokerGame - 德州扑克引擎 + 游戏客户端

C++20 德州扑克（Texas Hold'em）游戏引擎，带命令行客户端和 Web 前端。

## 项目结构

```
PokerGame/
├── core/                  # 核心引擎（纯 C++，无 UI 依赖）
│   ├── card.hpp/cpp       # 扑克牌（Rank/Suit/Card）
│   ├── deck.hpp/cpp       # 牌组（洗牌、发牌）
│   ├── hand_evaluator.hpp/cpp  # 牌型评估（5 牌 + 7选5 最优）
│   ├── player.hpp/cpp     # 玩家（筹码、手牌、状态）
│   └── game_engine.hpp/cpp # 游戏引擎（盲注→翻牌→转牌→河牌→比牌→边池）
├── tests/
│   ├── test_evaluator.cpp # 牌型评估单元测试（25 项）
│   ├── console_main.cpp   # 命令行游戏客户端（中文菜单）
│   ├── debug_engine.cpp   # 引擎调试程序
│   └── ws_client_test.cpp # WebSocket 客户端测试
├── lib/                   # 第三方头文件
│   ├── httplib.h          # HTTP/WebSocket 服务器
│   └── json.hpp           # JSON 解析（nlohmann）
├── server/
│   └── server.cpp         # WebSocket 游戏服务器（浏览器前端）
├── web/
│   └── index.html         # Canvas 前端界面
└── CMakeLists.txt
```

## 快速开始

### 命令行游戏（推荐先测这个）

```powershell
g++ -std=c++20 -I. core/*.cpp tests/console_main.cpp -o poker.exe -lws2_32
.\poker.exe
```

**主菜单功能：**
- 选择游戏人数（2-9 人）
- 选择模式：人机对战 / 观战（看 AI 互打）
- 每个 AI 独立设置难度（简单/普通/困难）
- 选择盲注等级（微额/低额/中额/高额/自定义）
- 筹码设置（500 / 1000 / 10000 / 自定义最高 10 亿）

**游戏内操作：** `f` 弃牌 | `c` 跟注 | `k` 过牌 | `r 金额` 加注 | `a` 全下

**观战模式：** 显示每个 AI 的手牌强度、胜率估算、底池赔率、决策推理

### 运行单元测试

```powershell
g++ -std=c++20 -I. core/*.cpp tests/test_evaluator.cpp -o test.exe
.\test.exe
```

### Web 服务器（浏览器前端）

```powershell
g++ -std=c++20 -I. -Ilib server/server.cpp core/*.cpp -o poker_server.exe -lws2_32
.\poker_server.exe
# 浏览器打开 http://localhost:8080
```

## AI 难度说明

| 等级 | 风格 | 翻牌前跟注率 | 加注率 | 诈唬率 |
|------|------|-------------|--------|--------|
| 1 简单 | 鱼（跟注站） | 95% | 5% | 0% |
| 2 普通 | 娱乐玩家 | 85% | 15% | 3% |
| 3 困难 | 紧凶型 | 70% | 30% | 10% |

AI 决策基于：
- **手牌胜率估算**（翻牌前对不同 rank/gap/suited 组合的胜率）
- **底池赔率计算**（跟注金额 / 底池）
- 赔率合适时跟注，牌力太弱时弃牌，偶尔诈唬

## 发牌机制

所有牌局使用随机洗牌（`std::mt19937` + `std::shuffle`），**不存在"根据 AI 水平分配特定牌"的后门**。AI 水平只影响决策策略，不影响发牌。牌组在每局开始时重新洗牌，公牌按德州扑克标准规则逐步翻出（翻牌 3 张 → 转牌 1 张 → 河牌 1 张）。

## 编译要求

- C++20
- g++ (GCC) 13+ 或 MSVC 2022+
- Windows: 链接 `ws2_32`
- 依赖全部为 header-only，无需额外安装

## 待办

- [ ] 联网多人对战（TCP/WebSocket 多房间）
- [ ] 前端界面完善（Canvas 动画效果）
- [ ] AI 进化（基于 GTO/纳什均衡的混合策略）
- [ ] 比赛模式/锦标赛结构

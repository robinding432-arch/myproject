# 社区工具 & 网页端设计文档

> 版本：v6.3 | 状态：设计稿

---

## 一、社区工具全景

| 工具 | 形式 | 用户 | 价值 |
|---|---|---|---|
| 🌌 星图网页版 | Web App | 所有玩家 | 查看星系/规划航线/分享发现 |
| 📊 玩家统计面板 | Web Dashboard | 个人 | 战绩/经济/成就总览 |
| 🏆 排行榜 | Web + 游戏内 | 所有玩家 | 周榜/季榜/全服 |
| 🛠️ Mod 工坊 | Web + 游戏内 | Mod 作者 | 上传/下载/评分 Mod |
| 📝 任务编辑器 | 桌面 App | 高级玩家 | 可视化创建自定义任务 |
| 🎨 飞船涂装器 | Web App | 所有玩家 | 浏览器里设计飞船涂装 |
| 📡 API 开放平台 | REST API | 开发者 | 第三方工具/机器人 |

---

## 二、星图网页版

### 2.1 功能

```
✅ 全星系 3D 可视化（WebGL）
✅ 点击星球 → 显示：名称/Biome/派系/威胁等级/物价
✅ 航线规划（输入起点终点 → 显示跃迁路线+燃料消耗）
✅ 玩家标记（好友在线位置）
✅ 截图分享（生成星球美图）
✅ 嵌入游戏内浏览器（M menu 打开）
```

### 2.2 技术栈

```
前端：React + Three.js (WebGL)
后端：Node.js + Express + PostgreSQL
数据库：PostgreSQL（星系数据）+ Redis（实时位置）
部署：Docker + Nginx
```

### 2.3 数据接口

```json
// GET /api/v1/galaxy/{galaxy_id}/stars
{
    "stars": [
        {
            "id": "star_001",
            "name": "Sol-7b",
            "type": "G-Type",
            "position": [120000, -45000, 89000],
            "temperature": 5778,
            "planets": [
                {
                    "id": "planet_042",
                    "name": "Aethelgard",
                    "biome": "Desert",
                    "faction": "Empire",
                    "threatLevel": 3,
                    "marketPrices": {
                        "iron_ore": 45,
                        "plasma_cell": 120
                    }
                }
            ]
        }
    ]
}
```

### 2.4 游戏内嵌入

```cpp
// WebBrowser 组件
void AStellarPlayerController::OpenGalaxyMap()
{
    UWebBrowser* Browser = NewObject<UWebBrowser>();
    Browser->SetInitialURL(TEXT("https://starmap.stellarsystem.game/"));
    Browser->AddToViewport();
}
```

---

## 三、玩家统计面板

### 3.1 数据模型

```json
{
    "playerId": "steam_76561198000000000",
    "displayName": "StellarVoyager",
    "stats": {
        "totalPlaytime": 482000,
        "planetsVisited": 47,
        "shipsDestroyed": 23,
        "deaths": 8,
        "tradeProfit": 1250000,
        "miningYield": 890000,
        "questsCompleted": 156,
        "factionReputation": {
            "Empire": 850,
            "Pirate": -300,
            "Merchant": 600,
            "Science": 400,
            "Native": 150
        },
        "pvpKDRatio": 2.8,
        "longestWarp": 45000000,
        "discoveries": ["planet_001", "star_042"]
    }
}
```

### 3.2 后端聚合

```sql
-- 每小时聚合统计
INSERT INTO player_stats_aggregated
SELECT
    player_id,
    DATE_TRUNC('hour', timestamp) AS hour,
    COUNT(*) FILTER (WHERE event_type = 'kill') AS kills,
    COUNT(*) FILTER (WHERE event_type = 'death') AS deaths,
    SUM(amount) FILTER (WHERE event_type = 'trade_sell') AS profit
FROM game_events
WHERE timestamp > NOW() - INTERVAL '24 hours'
GROUP BY player_id, hour;
```

---

## 四、排行榜系统

### 4.1 榜单类型

| 榜单 | 刷新频率 | 数据来源 |
|---|---|---|
| 击杀榜（周） | 每周一 0 点 | PvP 击杀数 |
| 贸易榜（周） | 每周一 0 点 | 贸易净利润 |
| 探索榜（季） | 每赛季结束 | 新发现星球数 |
| 派系贡献（实时） | 每小时 | 派系声望增量 |
| 财富榜（实时） | 每小时 | 净资产 |
| 跃迁距离（全服） | 实时 | 单次最长跃迁 |

### 4.2 游戏内展示

```
┌──────────────────────────────────────┐
│  🏆 排行榜           [周榜|季榜|全服] │
├──────────────────────────────────────┤
│  #1  StellarVoyager  ⚔ 47 击杀    │
│  #2  NebulaHunter   ⚔ 42 击杀    │
│  #3  VoidWalker     ⚔ 38 击杀    │
│  #4  ...                            │
│  #5  ...                            │
│  ── 你的排名 ──                    │
│  #23  You            ⚔ 12 击杀    │
└──────────────────────────────────────┘
```

### 4.3 防作弊

```
✅ 服务端权威统计（客户端只上报事件）
✅ 异常检测（1 小时杀 200 人 → 自动标记审查）
✅ 录像自动保存（上榜者最近 10 场）
✅ 人工审核（GM 后台）
```

---

## 五、Mod 工坊

### 5.1 功能

```
✅ 浏览/搜索/筛选 Mod
✅ 一键订阅 → 游戏内自动下载
✅ 评分/评论/截图
✅ 作者收益分成（Premium Mod）
✅ 依赖自动解析
✅ 版本兼容检查
```

### 5.2 数据结构

```json
{
    "modId": "better-trade-routes-v2",
    "name": "Better Trade Routes",
    "author": "EconMaster",
    "version": "2.1.0",
    "category": "Gameplay",
    "tags": ["economy", "trade", "prices"],
    "downloads": 15420,
    "rating": 4.7,
    "compatibility": ["v6.2", "v6.3"],
    "dependencies": ["base-economy-v1"],
    "previewImages": ["url1", "url2"],
    "description": "动态调整贸易价格曲线...",
    "changelog": "v2.1: 修复价格波动溢出"
}
```

### 5.3 游戏内集成

```cpp
// ModBrowser Widget
UCLASS()
class UModBrowserWidget : public UUserWidget
{
    UFUNCTION(BlueprintCallable)
    void FetchModList(const FString& Category, int32 Page);

    UFUNCTION(BlueprintCallable)
    void SubscribeMod(const FString& ModID);

    UFUNCTION(BlueprintCallable)
    void UnsubscribeMod(const FString& ModID);

    UFUNCTION(BlueprintCallable)
    void UpdateAllMods();
};
```

---

## 六、任务编辑器（桌面 App）

### 6.1 可视化编辑

```
┌──────────────────────────────────────────┐
│  📝 任务编辑器                          │
├──────────┬───────────────────────────────┤
│ 任务节点  │  属性面板                    │
│          │                              │
│ [开始]   │  任务名称: 护送商船          │
│   ↓      │  派系: Merchant              │
│ [对话]   │  难度: ★★★☆☆               │
│   ↓      │  奖励: 5000 Cr              │
│ [击杀×3] │  道德: +Lawful               │
│   ↓      │                              │
│ [护送]   │  条件:                       │
│   ↓      │  - 击杀 3 个海盗            │
│ [对话]   │  - 护送 NPC 到目标星球      │
│   ↓      │                              │
│ [完成]   │  [+ 添加条件]               │
│          │                              │
├──────────┴───────────────────────────────┤
│ [导出 JSON] [测试任务] [上传到工坊]      │
└──────────────────────────────────────────┘
```

### 6.2 导出格式

```json
{
    "questId": "custom_escort_001",
    "name": "护送商船",
    "faction": "Merchant",
    "difficulty": 3,
    "moralTag": "Lawful",
    "reward": {
        "credits": 5000,
        "reputation": 200,
        "items": ["trade_permit_merchant"]
    },
    "stages": [
        {
            "type": "Dialogue",
            "npc": "merchant_captain",
            "text": "我们需要护送到 Aethelgard..."
        },
        {
            "type": "KillTarget",
            "count": 3,
            "targetFaction": "Pirate"
        },
        {
            "type": "Escort",
            "targetNPC": "merchant_ship",
            "destinationPlanet": "planet_aethelgard"
        },
        {
            "type": "Dialogue",
            "npc": "merchant_captain",
            "text": "感谢你的护送！这是你的报酬。"
        }
    ]
}
```

---

## 七、飞船涂装器（Web）

### 7.1 功能

```
✅ 3D 实时预览（Three.js + WebGL）
✅ 选区着色（船体/机翼/引擎/武器）
✅ 材质参数（金属度/粗糙度/发光）
✅ 图案叠加（条纹/迷彩/派系标志）
✅ 导出为游戏内可用格式
✅ 分享链接/社区投票
```

### 7.2 数据流

```
Web 涂装器 → 导出 JSON 涂装数据
    ↓
上传到工坊 / 保存到本地
    ↓
游戏内 ModLoader 读取
    ↓
动态生成材质实例 → 应用到飞船
```

### 7.3 涂装数据格式

```json
{
    "paintId": "my_custom_paint",
    "name": "Solar Flare",
    "regions": {
        "hull": {
            "baseColor": "#FF4500",
            "metallic": 0.8,
            "roughness": 0.3,
            "emissive": "#FF2200",
            "pattern": "stripes_diagonal",
            "patternColor": "#FFD700"
        },
        "wings": {
            "baseColor": "#1A1A2E",
            "metallic": 0.9,
            "roughness": 0.1,
            "pattern": "faction_empire_logo"
        },
        "engine": {
            "baseColor": "#333333",
            "emissive": "#00FFFF",
            "glowIntensity": 2.5
        }
    }
}
```

---

## 八、开放 API 平台

### 8.1 API 端点

| 端点 | 方法 | 用途 |
|---|---|---|
| `/api/v1/player/{id}/stats` | GET | 玩家统计 |
| `/api/v1/galaxy/{id}/stars` | GET | 星系数据 |
| `/api/v1/market/{planet}/prices` | GET | 星球物价 |
| `/api/v1/leaderboard/{type}` | GET | 排行榜 |
| `/api/v1/mods/list` | GET | Mod 列表 |
| `/api/v1/events/stream` | WebSocket | 实时事件流 |

### 8.2 认证

```
OAuth 2.0 + Steam OpenID
限流：100 req/min/IP
WebSocket：1 连接/用户
```

### 8.3 第三方应用示例

```
✅ Discord 机器人（查玩家战绩/星球物价）
✅ 手机 App（拍卖行通知/好友上线提醒）
✅ 浏览器插件（游戏内 Wiki 联动）
✅ 交易计算器（Excel 插件）
✅ 社区统计网站（超越官方面板）
```

---

## 九、实施路线图

| 阶段 | 内容 | 时间 | 优先级 |
|---|---|---|---|
| P0 | 星图网页版 MVP | 2 周 | 🔴 |
| P0 | 玩家统计 API | 1 周 | 🔴 |
| P1 | 排行榜系统 | 2 周 | 🟡 |
| P1 | Mod 工坊后端 | 3 周 | 🟡 |
| P2 | 任务编辑器桌面版 | 4 周 | 🟢 |
| P2 | 飞船涂装器 Web | 3 周 | 🟢 |
| P3 | 开放 API 平台 | 2 周 | 🔵 |
| P3 | 第三方开发者文档 | 持续 | 🔵 |

---

## 十、技术架构图

```
                    ┌──────────────┐
                    │   Game Client │
                    └──────┬───────┘
                           │ HTTPS / WebSocket
                    ┌──────▼───────┐
                    │  API Gateway  │
                    └──────┬───────┘
              ┌──────┬─────┼─────┬──────┐
              │      │     │     │      │
        ┌─────▼─┐ ┌──▼──┐ ┌▼──┐ ┌▼──┐ ┌▼──────┐
        │Player  │ │Galaxy│ │Mkt│ │Mod│ │Events │
        │Service │ │Serv. │ │Srv│ │Srv│ │Stream │
        └───┬───┘ └──┬──┘ └┬──┘ └┬──┘ └──┬────┘
            │         │       │     │        │
        ┌───▼─────────▼──────▼─────▼────────▼───┐
        │         PostgreSQL + Redis Cluster       │
        └─────────────────────────────────────────┘
```

---

## 十一、运维注意事项

| 问题 | 方案 |
|---|---|
| 数据库压力 | 读写分离 + Redis 缓存热数据 |
| API 滥用 | 限流 + CAPTCHA + IP 黑名单 |
| Mod 恶意代码 | 沙箱执行 + 人工审核 + 社区举报 |
| DDoS | Cloudflare + 弹性扩容 |
| 数据泄露 | 最小权限 + 加密存储 + GDPR 合规 |
| 服务中断 | 多区域部署 + 自动故障转移 |

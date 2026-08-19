# ⚠️ 已知问题 & 修复方案

---

## 🔴 高优先级（影响功能）

### 1. NPC Spawn 返回空指针
- **位置**：`QuestSystemV2.cpp` → `SpawnQuestNPC()`
- **现象**：AI 生成的任务无法产生实际 NPC
- **修复**：创建 `BP_AIQuestCharacter` 蓝图（继承 `AAICharacter`），在函数中 `GetWorld()->SpawnActor<ABP_AIQuestCharacter>()`

### 2. 精炼 UI 缺失
- **位置**：`MiningSystem.h` → `FRefinedMaterial`
- **现象**：可以采矿但无法精炼
- **修复**：创建 `WBP_Refinery.umg`，绑定 `RefineRecipes` 数组，调用 `Server_Refine()`

### 3. 背包/货币接口未对接
- **位置**：`TradeSystem.cpp` → `Server_BuyFromPlayer/SellToPlayer`
- **现象**：贸易站价格显示正常但交易无效果
- **修复**：取消注释 `UCurrencyComponent*` 和 `UInventoryComponent*` 调用

---

## 🟡 中优先级（影响体验）

### 4. 矿脉 Mesh 为占位球体
- **修复**：美术创建 `SM_Ore_Vein_{OreType}.uasset`，通过资产覆盖层自动替换

### 5. 对话本地化缺失
- **位置**：`QuestSystemV2` 所有 `FText`
- **现象**：硬编码英文
- **修复**：编辑器 → Tools → Localization Dashboard → 添加语言 → 导出 PO 文件

### 6. 声望衰减未实现
- **位置**：`FactionManager` → 缺少 `DecayTimer`
- **现象**：声望永久不变
- **修复**：在 `Tick` 中每 3600 秒向 0 衰减 5%

### 7. 派系战争 AI 空实现
- **位置**：`FactionManager.cpp` → `UpdateWarfare()`
- **现象**：宣战后无实际效果
- **修复**：每 60 秒随机削减交战国声望 + 生成战斗任务

### 8. 任务链中间状态不存盘
- **位置**：`QuestChain::CurrentQuestIndex`
- **现象**：重启后链从头开始
- **修复**：在 `SaveSystem` 中序列化 `PlayerActiveChains`

---

## 🟢 低优先级（锦上添花）

### 9. 贸易网络 Tick 性能
- **现象**：100+ 站时每帧遍历有开销
- **优化**：改为 1Hz 更新（`if (fmod(GetWorld()->GetTimeSeconds(), 1.f) > DeltaTime) return;`）

### 10. 矿脉生成卡顿
- **现象**：200 个矿脉一次性生成 ~50ms 卡顿
- **优化**：改为 `AsyncTask` 分批，每帧 20 个

### 11. 对话树无内存池
- **现象**：大量程序化生成对话树占用内存
- **优化**：LRU 缓存，超出 50 棵自动释放

### 12. 派系领土无视觉表现
- **现象**：行星上无派系旗帜/颜色覆盖
- **修复**：在 `ProceduralPlanet` 的 Biome 着色中叠加派系颜色

### 13. 道德标签永久
- **现象**：玩家无法"改邪归正"
- **修复**：添加 `WashSins` 任务类型，完成后清除 `Savage/Ruthless` 标签

---

## 🔵 平台相关

### 14. Steam API 未初始化检查
- **位置**：`SteamIntegration.cpp`
- **现象**：非 Steam 构建会崩溃
- **修复**：所有 Steam 调用已用 `#if WITH_STEAMWORKS` 隔离 ✅

### 15. Dedicated Server 构建
- **现象**：当前未配置 DS target
- **修复**：添加 `StellarSystemServer.Target.cs`（参考 UE 官方 DS 模板）

---

## 修复进度

| 版本 | 已修复 | 新增已知 |
|---|---|---|
| v6.0 | 8/10 | 2 |
| v6.1 | 5/7 | 4 |
| **v6.2** | **3/8** | **15** |

> v6.2 新增 15 个已知问题（其中 3 个为高优先级），建议下个版本集中修复。

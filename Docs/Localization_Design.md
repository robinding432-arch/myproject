# 本地化系统设计文档

> 版本：v6.3 | 状态：设计稿

---

## 一、支持语言（首发）

| 语言 | 代码 | 优先级 | 工作量 |
|---|---|---|---|
| 简体中文 | zh-CN | P0 | 基准 |
| 英语 | en-US | P0 | 中→英 翻译 |
| 日语 | ja-JP | P1 | 中→日 翻译 |
| 韩语 | ko-KR | P1 | 中→韩 翻译 |
| 法语 | fr-FR | P2 | 外包 |
| 德语 | de-DE | P2 | 外包 |
| 西班牙语 | es-ES | P2 | 外包 |
| 俄语 | ru-RU | P3 | 外包 |
| 葡萄牙语 | pt-BR | P3 | 外包 |

---

## 二、技术架构

### 2.1 UE 本地化框架

```
使用 UE 内置 Localization Dashboard：
1. 所有 UI 文本用 NSLOCTEXT() 宏包裹
2. 所有 C++ 字符串用 LOCTEXT() 宏
3. 运行时切换 FInternationalization::SetCurrentCulture()
```

### 2.2 字符串表结构

```
Content/Localization/
├── Game_zh-CN.po
├── Game_en-US.po
├── Game_ja-JP.po
├── Game_ko-KR.po
├── Game_fr-FR.po
├── Game_de-DE.po
├── Game_es-ES.po
├── Game_ru-RU.po
└── Game_pt-BR.po
```

### 2.3 AI 生成文本标记

```cpp
// 所有 AI 生成的任务文本、NPC 对话需要走翻译管线
// 在 QuestSystemV2 里：

FString UQuestSystemV2::GenerateLocalizedText(const FString& TemplateKey,
    const FString& CultureCode)
{
    // 1. 查字符串表
    FString TableName = FString::Printf(TEXT("Game_%s"), *CultureCode);
    FString Result = NSLOCTEXT(*TableName, *TemplateKey, *TemplateKey).ToString();

    // 2. 如果找不到，fallback 到 en-US
    if (Result == TemplateKey)
    {
        Result = NSLOCTEXT("Game_en-US", *TemplateKey, *TemplateKey).ToString();
    }

    return Result;
}
```

---

## 三、需要本地化的内容分类

| 类别 | 示例 | 数量预估 |
|---|---|---|
| UI 文本 | 按钮/菜单/提示 | ~500 条 |
| 任务文本 | 描述/目标/对话 | ~2000 条（含 AI 生成） |
| NPC 对话 | 分支选项/闲聊 | ~3000 条 |
| 物品名称 | 武器/护甲/消耗品 | ~500 条 |
| 星球名称 | 程序化命名 | 动态生成 + 前缀表 |
| 系统消息 | 错误/提示/成就 | ~300 条 |
| 音频字幕 | 语音台词 | ~200 条 |

---

## 四、动态内容本地化策略

### 4.1 程序化星球名

```
中文：星辰 / 辉夜 / 苍穹 / 炎阳
英文：Stellar / Luminar / Celestia / Ignis
日文：星屑 / 輝夜 / 蒼穹 / 炎陽

生成规则：前缀表 × 后缀表 × 编号
例：zh-CN → "炎阳-VII"  en-US → "Ignis-VII"
```

### 4.2 AI 任务文本

```
模板（en-US）：
  "Deliver {item} to {npc} at {planet} before {time}"

翻译后（zh-CN）：
  "在{time}之前将{item}交给{planet}的{npc}"

占位符 {item} {npc} {planet} {time} 在翻译中保持位置灵活
```

### 4.3 数字/日期格式

```cpp
// 使用 UE 的 FCulture 自动处理
FText::AsNumber(1234567.89f)  // 自动按文化格式化
FText::AsDateTime(FDateTime::Now())
```

---

## 五、字体方案

| 语言 | 字体 | 说明 |
|---|---|---|
| 中文 | Noto Sans SC | Google Fonts 开源 |
| 英文 | Roboto | Android 默认，免费 |
| 日文 | Noto Sans JP | Google Fonts |
| 韩文 | Noto Sans KR | Google Fonts |
| 欧洲语言 | Roboto + 扩展字符集 | 覆盖法语/德语/西语/葡语 |
| 俄语 | Roboto + Cyrillic | 覆盖西里尔字母 |

> 所有字体用 **SDF（Signed Distance Field）** 渲染，支持动态分辨率。

---

## 六、音频本地化

| 语言 | 配音 | 字幕 |
|---|---|---|
| zh-CN | ✅ 首发 | ✅ |
| en-US | ✅ 首发 | ✅ |
| ja-JP | P1 | ✅ |
| ko-KR | P1 | ✅ |
| 其他 | ❌ 后期 | ✅（机翻审核） |

---

## 七、实现步骤

```
Step 1：把所有硬编码字符串改成 NSLOCTEXT()
Step 2：UE 编辑器 → Tools → Localization Dashboard
Step 3：添加目标文化（en-US, ja-JP, ko-KR...）
Step 4：收集文本 → 导出 PO 文件
Step 5：翻译 PO 文件（人工 + AI 辅助）
Step 6：导入翻译 → 编译
Step 7：运行时切换测试
Step 8：打包时勾选对应文化
```

---

## 八、代码接口

```cpp
// LocalizationManager.h（待创建）
UCLASS()
class ULocalizationManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    // 获取当前语言
    UFUNCTION(BlueprintCallable)
    FString GetCurrentLanguage() const;

    // 设置语言（自动保存到配置）
    UFUNCTION(BlueprintCallable)
    void SetLanguage(const FString& CultureCode);

    // 获取本地化字符串
    UFUNCTION(BlueprintCallable)
    FString GetLocalizedText(const FString& TableName, const FString& Key) const;

    // 格式化（带参数）
    UFUNCTION(BlueprintCallable)
    FString GetFormattedText(const FString& TableName, const FString& Key,
        const TArray<FString>& Args) const;

    // 本地化事件委托
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLanguageChanged, FString, NewLanguage);
    UPROPERTY(BlueprintAssignable)
    FOnLanguageChanged OnLanguageChanged;
};
```

---

## 九、测试清单

- [ ] 所有 UI 文本在 9 种语言下不溢出
- [ ] 从右到左语言（阿拉伯语/希伯来语）布局镜像
- [ ] 亚洲语言（中日韩）字体渲染清晰
- [ ] 音频字幕同步正确
- [ ] 数字/日期格式正确
- [ ] AI 生成文本翻译质量可接受
- [ ] 切换语言不需要重启游戏
- [ ] 存档中的语言设置正确恢复

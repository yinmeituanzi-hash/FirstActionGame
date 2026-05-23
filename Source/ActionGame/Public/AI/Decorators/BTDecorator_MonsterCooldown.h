#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Decorators/BTDecorator_Cooldown.h"
#include "BTDecorator_MonsterCooldown.generated.h"

/**
 * Sprint 4-C++ Day 7：怪物技能冷却 Decorator。
 *
 * 薄封装策略：直接继承 UE 自带 `UBTDecorator_Cooldown`，完全复用它的实现：
 *  - `CoolDownTime` 字段
 *  - `OnNodeDeactivation` 记录上次激活时间
 *  - `CalculateRawConditionValue` 冷却判断
 *  - `bTriggersAbort` 选项
 *
 * 我们只改两件事：
 *  1. `NodeName = "Monster Cooldown"`：BT 视图里跟其他系统的 Cooldown（如玩家技能、UI 提示）视觉上区分开
 *  2. `GetStaticDescription`：节点描述显示 "Monster Cooldown: 10.0s" 一眼可读
 *
 * 这条路线参考 010 `BTDecorator_CoolDownPlayer`——他们同样是继承官方版本加薄字段。
 *
 * 已知小细节：UE 自带版本的"start ready"语义依赖游戏时长。
 * `CalculateRawConditionValue` 是 `GetTimeSeconds() - LastUseTimestamp >= CoolDownTime`，
 * `LastUseTimestamp` 默认 0，当 `GetTimeSeconds() < CoolDownTime` 时（游戏前几秒内 spawn 的怪），
 * 第一次评估会被挡住。实战中玩家从主菜单进关卡再走到怪附近通常 ≥ 10s 不会踩中；
 * PIE 调试时如果怪在出生点旁立刻战斗可能短暂感受到"开局大招暂时不可用"，可忽略。
 *
 * 未来扩展挂载点：
 *  - 要加"按怪物属性缩放 CD"（参考 010 SkillSustain）：重写 `OnNodeActivation`，在调用 Super 前修改 `CoolDownTime`
 *  - 要加"怒气状态下 CD 减半"：同上，按 AlertState / Buff 改 CoolDownTime
 *  - 这些扩展都在本类里加字段 + 重写 1 个虚函数，不动 BT 资产
 *
 * 使用注意：
 *  - **`bTriggersAbort` 一般保持 false**（编辑器侧 `Observer Aborts` 保持 `None`）。
 *    开了会导致 CD 好的瞬间打断当前正在执行的分支强切大招，视觉上"普攻播一半突然变大招"很怪。
 *    保持 false 时，CD 好只是让下次 BT 选择时大招分支变得可用，自然衔接。
 */
UCLASS()
class ACTIONGAME_API UBTDecorator_MonsterCooldown : public UBTDecorator_Cooldown
{
	GENERATED_BODY()

public:
	UBTDecorator_MonsterCooldown();

protected:
	virtual FString GetStaticDescription() const override;
};

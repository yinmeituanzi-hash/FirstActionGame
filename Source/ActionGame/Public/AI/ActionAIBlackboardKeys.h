#pragma once

#include "CoreMinimal.h"

/**
 * Blackboard Key 名称集中管理。
 *
 * 为什么单独抽出来：
 * - C++ 里硬编码字符串容易拼错且难重构。
 * - 编辑器里 BB 资产里的 Key 名必须和这里一致。任何一边改名都要改两边。
 * - 这里集中维护后，BTService/BTTask/外部 Character 通通从这里取，重命名只改一处。
 *
 * 编辑器侧对应资产：Content/AI/Monster/BB_Monster_Basic
 *
 * 注：AlertState 从 Sprint 4-B+ Day 4 开始启用。
 */
namespace ActionAIBlackboardKeys
{
	/** 当前目标 Actor（玩家 / 仇恨最高者）。Type=Object(Actor)。 */
	const FName TargetActor = TEXT("TargetActor");

	/** 移动目标位置（巡逻点 / 噪音位置 / EQS 结果）。Type=Vector。 */
	const FName TargetLocation = TEXT("TargetLocation");

	/** 是否进入攻击距离。Type=Bool，由 BTService 计算后写入。 */
	const FName IsInAttackRange = TEXT("IsInAttackRange");

	/** 出生点（巡逻基点）。Type=Vector，OnPossess 后由 AIController 写一次。 */
	const FName HomeLocation = TEXT("HomeLocation");

	/** 当前警戒状态。Type=Enum(EAIAlertState)。Sprint 4-B+ 才用，现在是占位。 */
	const FName AlertState = TEXT("AlertState");

	/** 最近一次听到的声音位置。Type=Vector。Sprint 4-B++ 才用，现在是占位。 */
	const FName LastNoiseLocation = TEXT("LastNoiseLocation");
}

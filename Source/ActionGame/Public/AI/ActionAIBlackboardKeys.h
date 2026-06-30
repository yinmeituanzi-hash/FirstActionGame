#pragma once

#include "CoreMinimal.h"

/**
 * Blackboard Key 名称集中管理。
 *
 * 编辑器侧对应资产：Content/AI/Monster/BB_Monster_Basic。
 * C++ 的 BTService / BTTask 统一从这里取 Key，避免字符串拼错或重命名遗漏。
 */
namespace ActionAIBlackboardKeys
{
	/** 当前目标 Actor：玩家或仇恨最高者。Type=Object(Actor)。 */
	const FName TargetActor = TEXT("TargetActor");

	/** 移动目标位置：巡逻点 / 噪音位置 / EQS 结果。Type=Vector。 */
	const FName TargetLocation = TEXT("TargetLocation");

	/** 是否进入攻击距离。Type=Bool。旧攻击范围判断保留给过渡行为树使用。 */
	const FName IsInAttackRange = TEXT("IsInAttackRange");

	/** 当前战斗选招结果。Type=Name，由 BTService_PickCombatSkill 写入。 */
	const FName SelectedSkillId = TEXT("SelectedSkillId");

	/** 当前选中技能是否已经满足释放条件。Type=Bool。 */
	const FName IsSelectedSkillInRange = TEXT("IsSelectedSkillInRange");

	/** 当前目标是否在选中技能最大释放距离外，需要先靠近。Type=Bool。 */
	const FName ShouldApproachSelectedSkill = TEXT("ShouldApproachSelectedSkill");

	/** 当前选中技能释放距离。Type=Float。 */
	const FName SelectedSkillMinRange = TEXT("SelectedSkillMinRange");
	const FName SelectedSkillMaxRange = TEXT("SelectedSkillMaxRange");
	const FName SelectedSkillPreferredRange = TEXT("SelectedSkillPreferredRange");

	/** CD / 等待窗口内的战斗走位目标点。Type=Vector。 */
	const FName CombatMoveLocation = TEXT("CombatMoveLocation");

	/** 出生点 / 巡逻基点。Type=Vector，OnPossess 后由 AIController 写一次。 */
	const FName HomeLocation = TEXT("HomeLocation");

	/** 当前警戒状态。Type=Enum(EAIAlertState)。 */
	const FName AlertState = TEXT("AlertState");

	/** 最近一次听到的声音位置。Type=Vector。 */
	const FName LastNoiseLocation = TEXT("LastNoiseLocation");

	/**
	 * 是否被外部状态阻塞 AI 决策。Type=Bool。
	 * 由 BTService_AlertStateTick 从 Owner.HasActionTag(Block.AIControl) 同步。
	 */
	const FName IsBlocked = TEXT("IsBlocked");
}

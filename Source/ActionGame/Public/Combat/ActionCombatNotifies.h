#pragma once

#include "CoreMinimal.h"

/**
 * 攻击 / 战斗相关的 AnimNotify 名称集中管理。
 *
 * Montage 编辑器里加的 Notify 名必须和这里完全一致。任一边改名都要改两边。
 *
 * 设计原则：
 *  - 命中类 Notify（HitCheck）玩家和怪物**共用同名**，因为命中判定是相同概念。
 *  - 玩家专属 Notify（连段窗口、闪避取消窗口）保留前缀 `Attack` 不影响。
 */
namespace ActionCombatNotifies
{
	/**
	 * 攻击命中检测：玩家和怪物攻击 Montage 都用此 Notify 触发球形判定。
	 * 触发时，Owner 应该已经监听 OnPlayMontageNotifyBegin 并调 ActionCombatLibrary::PerformSphereAttackHit。
	 */
	const FName AttackHitCheck = TEXT("AttackHitCheck");

	/** 玩家专属：打开连段窗口（玩家可在窗口内按 Attack 切下一段）。 */
	const FName AttackComboWindowStart = TEXT("AttackComboWindowStart");

	/** 玩家专属：打开闪避取消窗口（移除 Block.Dodge）。 */
	const FName AttackDodgeCancelStart = TEXT("AttackDodgeCancelStart");
}

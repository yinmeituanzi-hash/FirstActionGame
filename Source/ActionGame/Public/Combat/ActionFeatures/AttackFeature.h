#pragma once

#include "CoreMinimal.h"
#include "Combat/ActionFeatures/MontageActionFeature.h"
#include "AttackFeature.generated.h"

class UAnimMontage;
class AActor;

/**
 * UAttackFeature
 *
 * 普通攻击连段动作。
 *
 * 职责：
 *   - 持有 N 段连击蒙太奇数组。
 *   - 维护当前连段索引（CurrentComboIndex）。
 *   - 处理 3 种 Notify：
 *       AttackHitCheck         → 命中判定
 *       AttackComboWindowStart → 打开连段窗口（玩家可在窗口内按 Attack 切下一段）
 *       AttackDodgeCancelStart → 打开闪避取消窗口（移除 Block.Dodge）
 *
 * 与原 PlayerCharacter 实现的对应：
 *   - StartAttackComboAtIndex / TryStartNextComboAttack → 移到本 Feature。
 *   - HandleAttackHitCheck                              → 移到本 Feature。
 *   - HitActorsThisAttack                               → 改为本 Feature 字段。
 *   - 连段切换的 BlendOut 由 ActionCombatComponent 协助完成（保留组件 API）。
 *
 * 升级路径：
 *   Sprint 5 过渡期暂时保留本类，避免普攻功能在 SkillNode 尚未完成前断掉。
 *   Day5 / Day6 之后，普攻连段、命中、Notify 窗口都应迁入 SkillObject + SkillNode + SkillEffect。
 *   迁移完成后删除 AttackFeature，不再继续往这里添加新的攻击逻辑。
 */
UCLASS(Blueprintable, ClassGroup = (Action))
class ACTIONGAME_API UAttackFeature : public UMontageActionFeature
{
	GENERATED_BODY()

public:
	UAttackFeature();

	// ---------- 蒙太奇配置 ----------

	/** 连段蒙太奇数组：索引 0/1/2/3 = 普攻第 1/2/3/4 段。可少于 4 段。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Animation")
	TArray<TObjectPtr<UAnimMontage>> ComboMontages;

	/** 当连段数组为空且没传入 ComboIndex 时使用的回退蒙太奇。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Animation")
	TObjectPtr<UAnimMontage> FallbackAttackMontage;

	// ---------- 命中配置 ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|HitCheck", meta = (ClampMin = "0.0"))
	float HitCheckRadius = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|HitCheck")
	float HitCheckForwardOffset = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|HitCheck")
	bool bDrawDebugHitSphere = true;

	// ---------- 状态 ----------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attack|State")
	int32 CurrentComboIndex = INDEX_NONE;

	UFUNCTION(BlueprintPure, Category = "Attack|State")
	int32 GetCurrentComboIndex() const { return CurrentComboIndex; }

	// ---------- Lifecycle ----------

	virtual void Execute() override;
	virtual void OnNotify(FName NotifyName, const FBranchingPointNotifyPayload& Payload) override;
	virtual void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted) override;

	/**
	 * 给输入层用：玩家在连段窗口内按了 Attack。
	 * @return true 表示成功切到下一段；false 表示当前不在窗口内或没有下一段。
	 */
	bool TryAdvanceCombo();

private:
	void StartComboAtIndex(int32 ComboIndex);
	UAnimMontage* GetMontageForComboIndex(int32 ComboIndex) const;
	int32 GetNextComboIndex() const;
	void HandleHitCheck();
	void HandleComboWindowStart();
	void HandleDodgeCancelStart();

	/** 防止同一段攻击对同一目标多次扣血。 */
	TSet<TWeakObjectPtr<AActor>> HitActorsThisSwing;
};

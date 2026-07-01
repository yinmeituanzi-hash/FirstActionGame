#pragma once

#include "CoreMinimal.h"
#include "Char/ActionCharacterBase.h"
#include "Combat/LockOn/ActionLockableInterface.h"
#include "ActionMonsterCharacter.generated.h"

class UAnimMontage;
class UAlertComponent;
class UAIBudgetComponent;
class UAIMoveLogicComponent;
class UAISignificanceComponent;
class UNoiseListenerComponent;
struct FTimerHandle;

/**
 * 迁移期最小怪物角色。
 *
 * 当前职责：
 * 1. 作为玩家攻击的受击对象
 * 2. 提供简单受伤日志
 * 3. 提供简单死亡反馈
 *
 * 后续再逐步接入 AI、受击表现、死亡动画和更正式的战斗逻辑。
 */
UCLASS()
class ACTIONGAME_API AActionMonsterCharacter : public AActionCharacterBase, public IActionLockableInterface
{
	GENERATED_BODY()

public:
	AActionMonsterCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void ApplyDamage(float InDamage) override;
	virtual void Die() override;

	// ---------- Sprint 4-A Day 2: AI 接口 ----------
	//
	// 这一组接口供 BTTask / BTService 使用，把"怪物当前能不能做某事 / 在做什么"
	// 暴露成稳定 API，让 BT 节点不需要直接读 Character 的内部字段或 GameplayTag。

	/** 当前是否正在执行攻击/技能动作。AIBudget 和 Significance 用它保护战斗动作不被睡死。 */
	UFUNCTION(BlueprintPure, Category = "Action|AI")
	bool IsAttacking() const;

	/** 距某 Actor 的水平距离（XY 平面），AI 用来判断是否进入攻击范围 / 追击范围。 */
	UFUNCTION(BlueprintPure, Category = "Action|AI")
	float GetDistance2DTo(const AActor* Other) const;

	/** 当前是否在配置的攻击距离内可以攻击给定目标。 */
	UFUNCTION(BlueprintPure, Category = "Action|AI")
	bool IsTargetInAttackRange(const AActor* Target) const;

	UFUNCTION(BlueprintPure, Category = "Action|AI")
	float GetMonsterAttackRange() const { return MonsterAttackRange; }

	// ---------- Sprint 4-B+ / 4-C+++: Alert Component ----------

	UFUNCTION(BlueprintPure, Category = "Action|AI|Alert")
	UAlertComponent* GetAlertComponent() const { return AlertComponent; }

	UFUNCTION(BlueprintPure, Category = "Action|AI|Movement")
	UAIMoveLogicComponent* GetAIMoveLogicComponent() const { return AIMoveLogicComponent; }

	UFUNCTION(BlueprintPure, Category = "Action|AI|Significance")
	UAISignificanceComponent* GetSignificanceComponent() const { return SignificanceComponent; }

	UFUNCTION(BlueprintPure, Category = "Action|AI|Budget")
	UAIBudgetComponent* GetBudgetComponent() const { return BudgetComponent; }

	// ---------- Sprint 4-C+ Day 6: 简化仇恨列表 ----------
	//
	// 最简版仇恨：只记录"攻击者 → 累计伤害"，不做衰减、不区分仇恨来源。
	// 单玩家场景下退化为"优先攻击者"，但仇恨表接口已经做好，未来加同伴 / 多人时
	// BTService_PickHatredTarget 切目标的逻辑不用改。
	//
	// 输入入口：HitReceiverComponent::ReceiveHit 末尾调 AddHatred(Attacker, DamageAmount)。
	// 输出入口：BTService_PickHatredTarget 调 GetHighestHatredTarget() 写回 BB.TargetActor。
	// 清空时机：死亡 / 离开 Combat（Combat→Alert 时仇恨已经无意义，下次进 Combat 重新累积）。

	/** 累加指定 Actor 的仇恨值（一般等于本次受到的伤害量）。Source 为空 / 死亡时静默忽略。 */
	UFUNCTION(BlueprintCallable, Category = "Action|AI|Hatred")
	void AddHatred(AActor* Source, float Value);

	/** 返回仇恨最高的有效目标。返回前会顺手清理无效 / 死亡条目。无候选返回 nullptr。 */
	UFUNCTION(BlueprintCallable, Category = "Action|AI|Hatred")
	AActor* GetHighestHatredTarget() const;

	/** 返回当前仇恨表大小（去除无效项前的原始数量，仅用于 Debug）。 */
	UFUNCTION(BlueprintPure, Category = "Action|AI|Hatred")
	int32 GetHatredEntryCount() const;

	/** 清空仇恨。死亡时自动调一次。 */
	UFUNCTION(BlueprintCallable, Category = "Action|AI|Hatred")
	void ClearHatred();

	/** Debug 用：返回 Top N 仇恨条目（按值降序）。用于 AI.HatredDebug 画怪头顶。 */
	struct FHatredEntry { TWeakObjectPtr<AActor> Target; float Value = 0.0f; };
	void GetHatredTopEntries(int32 N, TArray<FHatredEntry>& OutEntries) const;

	// ---------- IActionLockableInterface ----------

	virtual bool CanBeLockedOn_Implementation() const override;
	virtual FVector GetLockOnTargetLocation_Implementation() const override;
	virtual void OnLockedOn_Implementation() override;
	virtual void OnLockedOff_Implementation() override;

	/** 锁定标记 socket 名（默认 chest 之类）。配置在蒙太奇骨骼里有则使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|LockOn")
	FName LockOnSocketName = NAME_None;

	UFUNCTION(BlueprintPure, Category = "Action|LockOn")
	bool IsBeingLockedOn() const { return bIsBeingLockedOn; }

private:
	bool bIsBeingLockedOn = false;

protected:
	/** 拥有警戒状态、可疑位置、状态移动速度以及战斗播报响应 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Alert", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAlertComponent> AlertComponent;

	/** 怪物移动意图和速度落地组件。Alert / BTTask / BTService 只写意图，速度和 Strafe 状态由它统一应用。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIMoveLogicComponent> AIMoveLogicComponent;

	/** 怪物重要度 / TickLOD 组件。Character 只持有组件，具体降级策略归组件自身维护。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISignificanceComponent> SignificanceComponent;

	/** 全局 Tick 配额接入组件。注册、排序结果与切换节流均由 Budget 域维护。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Budget", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIBudgetComponent> BudgetComponent;

	// ---------- Day 5: 听觉感知 ----------

	/** 怪物的"耳朵"。挂在 Owner 上，BeginPlay 自动注册到 AINoiseSubsystem。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Hearing", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNoiseListenerComponent> NoiseListener;

	/** 临时通用攻击距离。后续会迁到 Skill 释放距离，当前仍供 BT Range Decorator / MoveTo 停止距离使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack", meta = (ClampMin = "0.0"))
	float MonsterAttackRange = 200.0f;

private:
	/**
	 * 仇恨表：Attacker → 累计伤害。
	 * 不加 UPROPERTY：TMap<TWeakObjectPtr, float> 反射支持有限；GC 由 TWeakObjectPtr 自身处理，
	 * GetHighestHatredTarget 会顺手清理失效项。
	 */
	TMap<TWeakObjectPtr<AActor>, float> HatredMap;

protected:
	/** 死亡时播放的最小死亡蒙太奇。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

	/** 怪物死亡后保留多久再自动销毁。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (ClampMin = "0.0"))
	float DeathLifeSpan = 2.0f;

	/** 在死亡动画真正结束前多久开始冻结姿势，避免先混出回到 idle。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (ClampMin = "0.0"))
	float DeathPoseFreezeLeadTime = 0.1f;

	/** 用于在死亡动画末尾冻结姿势，避免回到待机。 */
	FTimerHandle FreezeDeathPoseTimerHandle;

	/** 将当前死亡姿势冻结住，直到 Actor 销毁。 */
	void FreezeDeathPose();

};

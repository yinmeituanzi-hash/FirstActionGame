#pragma once

#include "CoreMinimal.h"
#include "Char/ActionCharacterBase.h"
#include "Combat/LockOn/ActionLockableInterface.h"
#include "Combat/HitReact/HitReactTypes.h"
#include "ActionMonsterCharacter.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAlertComponent;
class UAIBudgetComponent;
class UAISignificanceComponent;
class UNoiseListenerComponent;
struct FBranchingPointNotifyPayload;
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

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Action|Combat")
	void StartMonsterAttack();

	// ---------- Sprint 4-A Day 2: AI 接口 ----------
	//
	// 这一组接口供 BTTask / BTService 使用，把"怪物当前能不能做某事 / 在做什么"
	// 暴露成稳定 API，让 BT 节点不需要直接读 Character 的内部字段或 GameplayTag。

	/** 当前是否正在执行攻击动作（Montage 还没播完、Cooldown 还没好都算）。 */
	UFUNCTION(BlueprintPure, Category = "Action|AI")
	bool IsAttacking() const;

	/**
	 * 攻击节奏冷却，避免 BTTask 一帧调一次 StartMonsterAttack。
	 * 上一次攻击后必须等待 AttackCooldown 秒才能再次发起。
	 */
	UFUNCTION(BlueprintPure, Category = "Action|AI")
	float GetAttackCooldownRemaining() const;

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

	/** 测试用怪物攻击距离。Day 4 先用于打通怪物攻击玩家的受击闭环，后续会迁到正式 AI/技能配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack", meta = (ClampMin = "0.0"))
	float MonsterAttackRange = 200.0f;

	/** 怪物攻击触发的受击类型。先支持在 BP 中切换 LightHit / HeavyHit，便于验证玩家受击表。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack")
	EHitReactType MonsterAttackReactType = EHitReactType::HitFly;

	/** 怪物攻击命中反馈强度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack", meta = (ClampMin = "0.0"))
	float MonsterAttackFeedbackScale = 1.0f;

	/** 命中点从玩家中心沿攻击方向回退的距离，避免特效生成在胶囊体正中心。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack", meta = (ClampMin = "0.0"))
	float MonsterAttackHitLocationBackstep = 40.0f;

	/** 是否在受击动画播放前让玩家转向攻击者。默认关闭，避免影响方向受击测试。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack")
	bool bMonsterAttackRotateVictimToAttacker = false;

	/** HitFly 测试用水平击飞力度。只有 MonsterAttackReactType=HitFly 时生效。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack|HitFly", meta = (ClampMin = "0.0"))
	float MonsterAttackHitFlyXYStrength = 800.0f;

	/** HitFly 测试用向上击飞力度。只有 MonsterAttackReactType=HitFly 时生效。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack|HitFly", meta = (ClampMin = "0.0"))
	float MonsterAttackHitFlyZStrength = 400.0f;

	/** 命中玩家时是否走 Ragdoll（HitFly + 物理布娃娃）。关闭时只走击飞 LaunchCharacter。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack|HitFly")
	bool bMonsterAttackUseRagdoll = true;

	/**
	 * 怪物攻击蒙太奇。**必须配置且必须含 AttackHitCheck Notify**。
	 * - 没配 Montage → StartMonsterAttack 拒绝
	 * - 配了但没 Notify → Montage 会播完但不会造成任何伤害（命中走 Notify 路径）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack")
	TObjectPtr<UAnimMontage> MonsterAttackMontage;

	/** 攻击冷却（秒）。BTTask 间隔小于此值的攻击请求会被跳过。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack", meta = (ClampMin = "0.0"))
	float AttackCooldown = 1.5f;

	// ---------- 球形命中判定参数（与玩家 AttackFeature 同款） ----------

	/** 球心从怪物中心沿 Forward 方向的偏移距离（cm）。一般略小于攻击距离。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack|HitCheck", meta = (ClampMin = "0.0"))
	float HitCheckForwardOffset = 120.0f;

	/** 球半径（cm）。和 ForwardOffset 一起决定"挥刀的覆盖范围"。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack|HitCheck", meta = (ClampMin = "0.0"))
	float HitCheckRadius = 120.0f;

	/** 是否在 PIE 中画 Debug 球可视化命中范围。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack|HitCheck")
	bool bDrawDebugHitSphere = true;

private:
	/**
	 * 仇恨表：Attacker → 累计伤害。
	 * 不加 UPROPERTY：TMap<TWeakObjectPtr, float> 反射支持有限；GC 由 TWeakObjectPtr 自身处理，
	 * GetHighestHatredTarget 会顺手清理失效项。
	 */
	TMap<TWeakObjectPtr<AActor>, float> HatredMap;

	// ---------- 攻击运行时状态 ----------

	/** 上一次攻击发起的 World Time（用于 Cooldown 计算）。 */
	float LastAttackStartTime = -1000.0f;

	/** 当前攻击 Montage 是否还在播。 */
	bool bAttackInFlight = false;

	/** 本段攻击已命中的目标集合（防多次扣血）。Montage 结束时清空。 */
	TSet<TWeakObjectPtr<AActor>> HitActorsThisAttack;

	/** Montage 播完时清攻击状态。 */
	void FinishMonsterAttack();

	/** Montage 结束委托回调，转发到 FinishMonsterAttack。 */
	UFUNCTION()
	void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/**
	 * 监听 OnPlayMontageNotifyBegin 路由：当 ActiveAttackMontage 播放期间收到
	 * AttackHitCheck Notify 时，调 ActionCombatLibrary 做球形判定。
	 *
	 * 玩家走 UMontageActionFeature 同款机制（绑 OnPlayMontageNotifyBegin），
	 * 怪物没有 Feature 系统，所以直接在 Character 上挂监听。
	 */
	UFUNCTION()
	void OnAnyMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

	/** 实际执行球形命中判定（被 OnAnyMontageNotifyBegin 调用）。 */
	void HandleAttackHitCheckNotify();

	/** OnPlayMontageNotifyBegin 是否已经绑定。BeginPlay 时绑一次后置 true。 */
	bool bMontageNotifyBound = false;

protected:
	virtual void BeginPlay() override;

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

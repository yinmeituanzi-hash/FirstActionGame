#pragma once

#include "CoreMinimal.h"
#include "Char/ActionCharacterBase.h"
#include "Combat/LockOn/ActionLockableInterface.h"
#include "Combat/HitReact/HitReactTypes.h"
#include "ActionMonsterCharacter.generated.h"

class UAnimMontage;
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
	/** 测试用怪物攻击距离。Day 4 先用于打通怪物攻击玩家的受击闭环，后续会迁到正式 AI/技能配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack", meta = (ClampMin = "0.0"))
	float MonsterAttackRange = 400.0f;

	/** 怪物攻击触发的受击类型。先支持在 BP 中切换 LightHit / HeavyHit，便于验证玩家受击表。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack")
	EHitReactType MonsterAttackReactType = EHitReactType::LightHit;

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

	/** Day 5 先作为入口开关，真正 Ragdoll 会在 Day 6 完整实现。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|MonsterAttack|HitFly")
	bool bMonsterAttackUseRagdoll = false;

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

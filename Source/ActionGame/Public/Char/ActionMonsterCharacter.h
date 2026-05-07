#pragma once

#include "CoreMinimal.h"
#include "Char/ActionCharacterBase.h"
#include "Combat/LockOn/ActionLockableInterface.h"
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

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
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
	/** 非致死受伤时播放的最小受击蒙太奇。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Animation")
	TObjectPtr<UAnimMontage> HitReactMontage;

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

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ActionLockableInterface.generated.h"

class AActor;

UINTERFACE(MinimalAPI, Blueprintable)
class UActionLockableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * IActionLockableInterface
 *
 * 任何能被玩家"锁定"的实体（怪物、可破坏物、Boss 弱点）都实现这个接口。
 *
 * 设计理由：
 *   - 不要让 LockOnComponent 直接 Cast<AActionMonsterCharacter>，
 *     否则将来加 BossPart / 可破坏物时还要回去改 LockOnComponent。
 *   - 接口只暴露最小必要 API：能否被锁、锁定点位置、被锁/解锁回调。
 */
class ACTIONGAME_API IActionLockableInterface
{
	GENERATED_BODY()

public:
	/**
	 * 当前是否处于"可被锁定"状态（活着 + 在玩家可视范围内可被发现）。
	 * 已经死亡或临时无敌的目标返回 false，会被 LockOnComponent 自动剔除。
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Action|LockOn")
	bool CanBeLockedOn() const;
	virtual bool CanBeLockedOn_Implementation() const { return true; }

	/**
	 * 玩家锁定该目标时使用的世界位置。
	 * 默认是 Actor 位置；怪物可 override 返回胸口/头部 socket 让相机更舒服。
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Action|LockOn")
	FVector GetLockOnTargetLocation() const;
	virtual FVector GetLockOnTargetLocation_Implementation() const { return FVector::ZeroVector; }

	/** 被玩家锁定时回调（可用于显示血条 / 锁定标记 UI）。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Action|LockOn")
	void OnLockedOn();
	virtual void OnLockedOn_Implementation() {}

	/** 被玩家解锁时回调。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Action|LockOn")
	void OnLockedOff();
	virtual void OnLockedOff_Implementation() {}
};

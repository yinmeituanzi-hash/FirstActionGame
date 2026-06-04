#pragma once

#include "CoreMinimal.h"
#include "AI/Budget/AIBudgetTypes.h"
#include "Components/ActorComponent.h"
#include "AIBudgetComponent.generated.h"

class AActionMonsterCharacter;
class UAISignificanceComponent;

/**
 * 单体怪物的 Budget 接入点。
 *
 * Subsystem 只负责全局排序和配额；本组件负责注册、状态切换节流，
 * 以及判断当前怪物能否被降级。类似 010 的 AIBudgetAllocateComponent。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UAIBudgetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIBudgetComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Action|AI|Budget")
	AActionMonsterCharacter* GetOwnerMonster() const;

	UFUNCTION(BlueprintPure, Category = "Action|AI|Budget")
	bool IsBudgetEnabled() const { return bBudgetEnabled; }

	UFUNCTION(BlueprintPure, Category = "Action|AI|Budget")
	EAIBudgetAllocationReason GetAllocationReason() const { return AllocationReason; }

	/** 攻击、受击、Ragdoll、锁定等关键表现期间，不允许 Budget 冻结该怪物。 */
	bool CanReduceWork() const;

	/**
	 * 应用全局分配结果。恢复立即生效；降级带短暂节流，避免配额边缘来回抖动。
	 * bForce 用于关闭 Budget 系统时立即清除它留下的限制。
	 */
	void ApplyBudgetEnabled(bool bEnabled, EAIBudgetAllocationReason Reason, bool bForce = false);

	FString GetProtectionReason() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Budget", meta = (AllowPrivateAccess = "true"))
	bool bBudgetEnabled = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Budget", meta = (AllowPrivateAccess = "true"))
	EAIBudgetAllocationReason AllocationReason = EAIBudgetAllocationReason::UnderBudget;

	/** 只节流 Enabled -> Disabled。恢复必须及时，避免靠近玩家后仍短暂停摆。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Budget", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DisableThrottleSeconds = 0.5f;

	float LastStateChangeTime = -1000.0f;

	UAISignificanceComponent* GetSignificanceComponent() const;
};

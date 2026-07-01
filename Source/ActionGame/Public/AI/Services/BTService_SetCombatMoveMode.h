#pragma once

#include "CoreMinimal.h"
#include "AI/Movement/AIMoveLogicComponent.h"
#include "BehaviorTree/BTService.h"
#include "BTService_SetCombatMoveMode.generated.h"

/**
 * 行为树分支级移动意图 Service。
 *
 * 进入分支时写 UAIMoveLogicComponent::CombatMoveMode，离开分支时清理。
 * 用它表达“正在追近技能释放距离”或“正在 Combat Strafe 走位”，
 * 让速度、动画 Strafe 判断不散落在各个 Service / Task 里。
 */
UCLASS()
class ACTIONGAME_API UBTService_SetCombatMoveMode : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_SetCombatMoveMode();

	UPROPERTY(EditAnywhere, Category = "Action|AI|Movement")
	EAICombatMoveMode CombatMoveMode = EAICombatMoveMode::None;

protected:
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};

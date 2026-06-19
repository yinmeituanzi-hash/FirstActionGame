#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPRecoverComponent.generated.h"

class UAttributeComponent;

/**
 * SP 自然回复组件。
 *
 * 独立于 AttributeComponent，负责"何时回复、回复多少"的业务逻辑。
 * AttributeComponent 只提供读写接口，不承载自然回复策略。
 * 后续如果有"战斗中暂停回复"、"受击打断回复 CD"等需求，都在这里扩展。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API USPRecoverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPRecoverComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** SP 每秒自然回复量。0 表示不自然回复。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|SP", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float SPRegenPerSecond = 0.0f;

	/** Timer 触发间隔（秒）。实际每次回复量 = SPRegenPerSecond * SPRegenInterval。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|SP", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float SPRegenInterval = 0.5f;

	FTimerHandle SPRegenTimerHandle;

	void TickSPRegen();
};

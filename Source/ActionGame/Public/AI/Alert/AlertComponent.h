#pragma once

#include "CoreMinimal.h"
#include "AI/ActionAITypes.h"
#include "Components/ActorComponent.h"
#include "AlertComponent.generated.h"

class AActionMonsterCharacter;

/**
* 单体怪物警戒域
* 该组件管控运行时警戒状态、可疑声响位置、状态移动速度，
* 以及战斗警戒信号的收发逻辑。涉及人工智能警戒状态的相关系统，
* 均应依托此组件实现功能，避免将怪物角色动作类臃肿化为警戒总接口。
*/
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UAlertComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAlertComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintAssignable, Category = "Action|AI|Alert")
	FAIAlertStateChangedSignature OnAlertStateChanged;

	UFUNCTION(BlueprintPure, Category = "Action|AI|Alert")
	AActionMonsterCharacter* GetOwnerMonster() const;

	UFUNCTION(BlueprintPure, Category = "Action|AI|Alert")
	EAIAlertState GetAlertState() const { return AlertState; }

	UFUNCTION(BlueprintCallable, Category = "Action|AI|Alert")
	void SetAlertState(EAIAlertState NewState);

	UFUNCTION(BlueprintPure, Category = "Action|AI|Alert")
	FVector GetLastNoiseLocation() const { return LastNoiseLocation; }

	UFUNCTION(BlueprintCallable, Category = "Action|AI|Alert")
	void SetLastNoiseLocation(const FVector& InLocation);

	UFUNCTION(BlueprintPure, Category = "Action|AI|Alert")
	float GetLastNoiseTime() const { return LastNoiseTime; }

	UFUNCTION(BlueprintPure, Category = "Action|AI|AlertBroadcast")
	float GetAlertBroadcastRadius() const { return AlertBroadcastRadius; }

	UFUNCTION(BlueprintCallable, Category = "Action|AI|AlertBroadcast")
	bool TryBroadcastCombatAlert(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "Action|AI|AlertBroadcast")
	bool ReceiveCombatAlert(AActor* Target, UAlertComponent* Source);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Alert", meta = (AllowPrivateAccess = "true"))
	EAIAlertState AlertState = EAIAlertState::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Alert", meta = (AllowPrivateAccess = "true"))
	FVector LastNoiseLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Alert", meta = (AllowPrivateAccess = "true"))
	float LastNoiseTime = -1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Alert|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float IdleMaxWalkSpeed = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Alert|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AlertMaxWalkSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Alert|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float CombatMaxWalkSpeed = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Alert", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AlertChangeCooldown = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|AlertBroadcast", meta = (AllowPrivateAccess = "true"))
	bool bEnableAlertBroadcast = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|AlertBroadcast", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AlertBroadcastRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|AlertBroadcast", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AlertBroadcastCooldown = 5.0f;

	float LastAlertStateChangeTime = -1000.0f;
	float LastAlertBroadcastTime = -1000.0f;

	void ApplyAlertStateMovementSettings();
};

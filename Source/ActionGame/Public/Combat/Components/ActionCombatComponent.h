#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "ActionCombatComponent.generated.h"

class ACharacter;
class UAnimInstance;
class UAnimMontage;

/**
 * Lightweight owner for action-combat runtime details that should not keep
 * growing inside AActionPlayerCharacter.
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UActionCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UActionCombatComponent();

	// Blend-out used when one attack Montage intentionally hands off to the next combo segment.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|Attack", meta = (ClampMin = "0.0"))
	float AttackComboMontageBlendOutTime = 0.08f;

	// Max yaw adjustment when the next combo attack consumes an attack turn window.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|Attack", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float AttackTurnMaxDegrees = 60.0f;

	// Short blend time used when the next combo attack consumes the attack turn window.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|Attack", meta = (ClampMin = "0.0"))
	float AttackTurnInterpDuration = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|Dodge", meta = (ClampMin = "1"))
	int32 MaxDodgeCharges = 2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat|Dodge")
	int32 CurrentDodgeCharges = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|Dodge", meta = (ClampMin = "0.0"))
	float DodgeChargeCooldown = 2.0f;

	bool StopAttackMontageForComboTransition(UAnimInstance* AnimInstance, UAnimMontage* PreviousAttackMontage);

	void ApplyAttackTurnAtComboStart(ACharacter* Character, const FVector2D& LastMoveInput);
	void ClearAttackTurnInterpolation();

	void InitializeDodgeCharges();
	bool HasAvailableDodgeCharge() const;
	void ConsumeDodgeCharge();
	int32 GetCurrentDodgeCharges() const { return CurrentDodgeCharges; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void StartAttackTurnInterpolation(ACharacter* Character, float TargetYaw);
	void UpdateAttackTurnInterpolation();
	void RestoreDodgeCharge();

	FTimerHandle AttackTurnInterpolationTimerHandle;
	FTimerHandle DodgeChargeRestoreTimerHandle;
	TWeakObjectPtr<ACharacter> AttackTurnCharacter;
	FRotator AttackTurnStartRotation = FRotator::ZeroRotator;
	FRotator AttackTurnTargetRotation = FRotator::ZeroRotator;
	float AttackTurnInterpolationElapsed = 0.0f;
};

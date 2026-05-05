#pragma once

#include "CoreMinimal.h"
#include "Char/ActionCharacterBase.h"
#include "ActionPlayerCharacter.generated.h"

class UAnimMontage;
class UCameraComponent;
class UInputAction;
class UInputBufferComponent;
class UInputMappingContext;
class USpringArmComponent;
struct FTimerHandle;
struct FBranchingPointNotifyPayload;
struct FInputActionValue;

/**
 * Player-controlled action character.
 *
 * Current responsibilities:
 * - Third-person camera setup.
 * - Enhanced Input binding.
 * - Minimal input buffering.
 * - Attack/dodge Montage playback and notify-driven action windows.
 */
UCLASS()
class ACTIONGAME_API AActionPlayerCharacter : public AActionCharacterBase
{
	GENERATED_BODY()

public:
	AActionPlayerCharacter(const FObjectInitializer& ObjectInitializer);
	virtual bool CanAttack() const override;

	UFUNCTION(BlueprintPure, Category = "Action|Combat")
	bool CanDodge() const;

protected:
	// Third-person camera boom. The spring arm keeps camera distance and follows controller yaw.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	// Actual follow camera attached to the boom.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	// Base third-person movement input context from the template project.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	// Combat input context. Expected to contain Attack and Dodge actions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> RuntimeCombatMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> DodgeAction;

	// Short-lived input buffer used by action windows, for example pressing Dodge slightly before cancel becomes legal.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputBufferComponent> InputBufferComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float InputBufferLifetime = 0.25f;

	// Current basic attack Montage. Hit timing is driven by Montage Notify, not by a fixed timer.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> AttackMontage;

	// Optional combo chain. Index 0-3 maps to normal attack segment 1-4.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Animation|Attack", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UAnimMontage>> AttackComboMontages;

	// Fallback dodge Montage if a directional Montage is not assigned.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> DodgeMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Animation|Dodge", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> DodgeForwardMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Animation|Dodge", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> DodgeLeftMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Animation|Dodge", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> DodgeBackwardMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Animation|Dodge", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> DodgeRightMontage;

	// Current prototype hit check uses a sphere in front of the character.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AttackHitCheckRadius = 120.0f;

	// Distance from character origin to the center of the melee hit sphere.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (AllowPrivateAccess = "true"))
	float AttackHitCheckForwardOffset = 120.0f;

	// Max yaw adjustment when the next combo attack consumes an attack turn window.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "180.0"))
	float AttackTurnMaxDegrees = 60.0f;

	// Short blend time used when the next combo attack consumes the attack turn window.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AttackTurnInterpDuration = 0.15f;

	// TODO(ActionTask): Temporary Montage Notify/Ended guard. Move this into ActionTask/ActionFeature/SkillObject later.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (AllowPrivateAccess = "true"))
	bool bAttackInProgress = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (AllowPrivateAccess = "true"))
	bool bAttackHitTriggeredThisSequence = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat|Attack", meta = (AllowPrivateAccess = "true"))
	int32 CurrentAttackComboIndex = INDEX_NONE;

	// TODO(ActionTask): Temporary Montage Notify/Ended guard. Move this into ActionTask/ActionFeature/SkillObject later.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (AllowPrivateAccess = "true"))
	bool bDodgeInProgress = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|Dodge", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 MaxDodgeCharges = 2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat|Dodge", meta = (AllowPrivateAccess = "true"))
	int32 CurrentDodgeCharges = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|Dodge", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DodgeChargeCooldown = 2.0f;

	// Prevents one attack swing from damaging the same target more than once.
	TSet<TWeakObjectPtr<AActor>> HitActorsThisAttack;

	// Kept even when movement is blocked, so action windows can still read the player's intended direction.
	FVector2D LastMoveInput = FVector2D::ZeroVector;

	TObjectPtr<UAnimMontage> ActiveAttackMontage;

	// Used to validate Dodge Montage ended callbacks against the currently active dodge.
	TObjectPtr<UAnimMontage> ActiveDodgeMontage;

	FTimerHandle DodgeChargeRestoreTimerHandle;
	FTimerHandle AttackTurnInterpolationTimerHandle;
	FRotator AttackTurnStartRotation = FRotator::ZeroRotator;
	FRotator AttackTurnTargetRotation = FRotator::ZeroRotator;
	float AttackTurnInterpolationElapsed = 0.0f;

protected:
	virtual void OnActionStateExit(EActionCharacterState OldState, EActionCharacterState NewState) override;
	virtual void OnActionStateEnter(EActionCharacterState OldState, EActionCharacterState NewState) override;
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void ClearMoveInput();
	void Look(const FInputActionValue& Value);
	void BuildRuntimeCombatInputMapping();

	bool TryConsumeAttackInput();
	bool TryConsumeDodgeInput();

	bool StartAttackComboAtIndex(int32 ComboIndex);
	UAnimMontage* GetAttackMontageForComboIndex(int32 ComboIndex) const;
	int32 GetNextAttackComboIndex() const;
	bool TryStartNextComboAttack();
	void OpenAttackTurnWindow();
	void ApplyAttackTurnAtComboStart();
	void StartAttackTurnInterpolation(float TargetYaw);
	void UpdateAttackTurnInterpolation();

	void BeginAttackSequence(int32 ComboIndex);
	void HandleAttackHitCheck();
	void EndAttackSequence();

	void TryStartDodge();
	void BeginDodgeSequence();
	void EndDodgeSequence();

	void ConsumeDodgeCharge();
	void RestoreDodgeCharge();
	bool HasAvailableDodgeCharge() const;

	FVector ResolveDodgeDirection() const;
	UAnimMontage* SelectDodgeMontage() const;

	UFUNCTION()
	void OnMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

	UFUNCTION()
	void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted);

public:
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	void OnAttackInput();

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	void OnDodgeInput();

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	void TryStartAttack();

	UFUNCTION(BlueprintPure, Category = "Action|Combat|Dodge")
	int32 GetCurrentDodgeCharges() const { return CurrentDodgeCharges; }

	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

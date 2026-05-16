#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "TimerManager.h"
#include "HitPhysicsComponent.generated.h"

class AActionCharacterBase;
class UAnimMontage;
class UCapsuleComponent;
class UCharacterMovementComponent;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EActionRagdollGetUpType : uint8
{
	Front UMETA(DisplayName = "Get Up From Front"),
	Back UMETA(DisplayName = "Get Up From Back")
};

/** Broadcast while the mesh is still in the final ragdoll pose. AnimBP should SnapshotPose immediately. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FActionRagdollSnapshotRequested);

/** Broadcast after the get-up montage starts. AnimBP should blend from the snapshot to the montage pose. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FActionGetUpStarted, EActionRagdollGetUpType, GetUpType, float, BlendDuration);

/** Broadcast when the get-up protected state ends. AnimBP should return fully to locomotion. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FActionGetUpFinished);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UHitPhysicsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHitPhysicsComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "Action|HitPhysics")
	bool CanApplyHitFly() const;

	UFUNCTION(BlueprintCallable, Category = "Action|HitPhysics")
	void ApplyHitImpulse(const FVector& HitDirection, float XYStrength, float ZStrength);

	UFUNCTION(BlueprintCallable, Category = "Action|HitPhysics")
	void StartRagdoll(const FVector& InitialImpulse);

	UFUNCTION(BlueprintCallable, Category = "Action|HitPhysics")
	void CancelGetUp();

	UFUNCTION(BlueprintPure, Category = "Action|HitPhysics")
	float GetRemainingHitFlyCooldown() const;

	UFUNCTION(BlueprintPure, Category = "Action|HitPhysics")
	bool IsRagdollActive() const { return bIsRagdollActive || bIsRagdollPending; }

	UFUNCTION(BlueprintPure, Category = "Action|HitPhysics")
	bool IsGettingUp() const { return bIsGettingUp; }

	UPROPERTY(BlueprintAssignable, Category = "Action|HitPhysics|GetUp")
	FActionRagdollSnapshotRequested OnRagdollSnapshotRequested;

	UPROPERTY(BlueprintAssignable, Category = "Action|HitPhysics|GetUp")
	FActionGetUpStarted OnGetUpStarted;

	UPROPERTY(BlueprintAssignable, Category = "Action|HitPhysics|GetUp")
	FActionGetUpFinished OnGetUpFinished;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics", meta = (ClampMin = "0.0"))
	float HitFlyCooldown = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics")
	bool bOverrideCurrentVelocityOnHitFly = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics")
	bool bRecoverMovementModeBeforeLaunch = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll", meta = (ClampMin = "0.0"))
	float RagdollStartDelay = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll", meta = (ClampMin = "0.0"))
	float MinimumRagdollTime = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll", meta = (ClampMin = "0.1"))
	float MaxRagdollTime = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll", meta = (ClampMin = "0.0"))
	float RagdollSettleSpeed = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll", meta = (ClampMin = "0.0"))
	float RagdollSettleAngularSpeed = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll", meta = (ClampMin = "0.0"))
	float RagdollGroundTraceDistance = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll", meta = (ClampMin = "0.0"))
	float RagdollGetUpGroundDistance = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll")
	bool bSyncCapsuleToPelvisDuringRagdoll = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|GetUp", meta = (ClampMin = "0.0"))
	float FallbackGetUpBlockDuration = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll")
	FName PelvisBoneName = TEXT("pelvis");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll")
	FName NeckBoneName = TEXT("neck_01");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll")
	FName RagdollCollisionProfileName = TEXT("Ragdoll");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll")
	TEnumAsByte<ECollisionChannel> RagdollGroundTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|GetUp")
	bool bInvertGetUpDirection = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|GetUp")
	TObjectPtr<UAnimMontage> GetUpFromFrontMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|GetUp")
	TObjectPtr<UAnimMontage> GetUpFromBackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|GetUp", meta = (ClampMin = "0.1"))
	float GetUpPlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|GetUp", meta = (ClampMin = "0.0"))
	float GetUpBlendDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|HitPhysics|Debug")
	bool bDrawDebugRagdoll = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Debug", meta = (ClampMin = "0.0", EditCondition = "bDrawDebugRagdoll"))
	float DebugDrawLifetime = 0.0f;

private:
	float LastHitFlyTime = -1.0f;
	float RagdollStartTime = -1.0f;

	bool bIsRagdollPending = false;
	bool bIsRagdollActive = false;
	bool bIsGettingUp = false;
	bool bIsRestoringMeshRelativeTransform = false;

	FVector PendingRagdollImpulse = FVector::ZeroVector;

	FTransform DefaultMeshRelativeTransform = FTransform::Identity;
	FVector DefaultMeshRelativeLocation = FVector::ZeroVector;
	TWeakObjectPtr<USceneComponent> DefaultMeshAttachParent;
	FName DefaultMeshAttachSocketName = NAME_None;
	bool bHasCachedMeshState = false;
	FTransform MeshRelativeRestoreStart = FTransform::Identity;
	float MeshRelativeRestoreElapsedTime = 0.0f;
	float MeshRelativeRestoreDuration = 0.0f;

	ECollisionEnabled::Type SavedCapsuleCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	FName SavedCapsuleCollisionProfileName = NAME_None;
	ECollisionEnabled::Type SavedMeshCollisionEnabled = ECollisionEnabled::NoCollision;
	FName SavedMeshCollisionProfileName = NAME_None;
	bool bSavedMeshGenerateOverlapEvents = false;

	FTimerHandle EnterRagdollTimerHandle;
	FTimerHandle FinishGetUpTimerHandle;

	AActionCharacterBase* GetOwnerCharacter() const;
	UCharacterMovementComponent* GetOwnerMovement() const;
	USkeletalMeshComponent* GetOwnerMesh() const;
	UCapsuleComponent* GetOwnerCapsule() const;

	void LaunchCharacterInternal(AActionCharacterBase* OwnerChar, UCharacterMovementComponent* Movement, const FVector& LaunchVelocity);
	void EnterRagdoll();
	void FinishRagdollAndStartGetUp();
	void FinishGetUp();

	bool IsRagdollReadyForGetUp(FHitResult& OutGroundHit) const;
	bool TraceGroundBelowPelvis(FHitResult& OutGroundHit) const;
	FVector GetPelvisWorldLocation() const;
	FVector GetNeckWorldLocation() const;
	bool IsRagdollFaceUp() const;
	EActionRagdollGetUpType DetermineGetUpType() const;
	UAnimMontage* GetGetUpMontage(EActionRagdollGetUpType GetUpType) const;
	FRotator ComputeCapsuleRotationFromNeckPelvis() const;
	void SyncCapsuleLocationToPelvis();
	void StartMeshRelativeTransformRestore(float Duration);
	void TickMeshRelativeTransformRestore(float DeltaTime);

	void AddRagdollBlockTags();
	void ClearRagdollBlockTags();
	void CacheDefaultMeshState();
	void ValidateRagdollSetup() const;
	void DrawDebugRagdoll() const;
	void SaveCurrentCollisionState();
	void RestoreCollisionState();
	void LogMeshState(const TCHAR* Phase) const;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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

/**
 * 受击物理层组件。
 *
 * Day 6 负责 HitFly Ragdoll、落地稳定检测、前后起身入口。
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UHitPhysicsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHitPhysicsComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 当前是否允许再次应用击飞。用于避免连续 HitFly 每帧重置速度。 */
	UFUNCTION(BlueprintPure, Category = "Action|HitPhysics")
	bool CanApplyHitFly() const;

	/** 非 Ragdoll 击飞入口：用 CharacterMovement / LaunchCharacter 推动角色。 */
	UFUNCTION(BlueprintCallable, Category = "Action|HitPhysics")
	void ApplyHitImpulse(const FVector& HitDirection, float XYStrength, float ZStrength);

	/** Ragdoll 击飞入口：进入强控制状态，启用 Mesh 物理，落地稳定后播放起身动画。 */
	UFUNCTION(BlueprintCallable, Category = "Action|HitPhysics")
	void StartRagdoll(const FVector& InitialImpulse);

	UFUNCTION(BlueprintPure, Category = "Action|HitPhysics")
	float GetRemainingHitFlyCooldown() const;

	UFUNCTION(BlueprintPure, Category = "Action|HitPhysics")
	bool IsRagdollActive() const { return bIsRagdollActive || bIsRagdollPending; }

	UFUNCTION(BlueprintPure, Category = "Action|HitPhysics")
	bool IsGettingUp() const { return bIsGettingUp; }

protected:
	/** 击飞冷却，防止同一个角色被 HitFly 连续命中时速度不断被重置。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics", meta = (ClampMin = "0.0"))
	float HitFlyCooldown = 0.35f;

	/** true 时，新击飞会清掉当前移动速度，让测试结果更稳定。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics")
	bool bOverrideCurrentVelocityOnHitFly = true;

	/** MovementMode 为 None 时先恢复为 Walking，否则 LaunchCharacter 不会正常写入位移。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics")
	bool bRecoverMovementModeBeforeLaunch = true;

	/** HitFly_Start 播放后延迟多久切入 Ragdoll。设为 0 可立刻进入布娃娃。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll", meta = (ClampMin = "0.0"))
	float RagdollStartDelay = 0.12f;

	/** Ragdoll 至少持续多久，避免刚启用物理就立刻判定落地起身。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll", meta = (ClampMin = "0.0"))
	float MinimumRagdollTime = 0.45f;

	/** 超过这个时间后即使没有完全稳定，也会尝试起身，防止角色永久躺地。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll", meta = (ClampMin = "0.1"))
	float MaxRagdollTime = 4.0f;

	/** 骨盆速度低于该值并且检测到地面时，认为 Ragdoll 已经稳定。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll", meta = (ClampMin = "0.0"))
	float RagdollSettleSpeed = 90.0f;

	/** 从骨盆向下检测地面的距离。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll", meta = (ClampMin = "0.0"))
	float RagdollGroundTraceDistance = 160.0f;

	/** 起身期间继续阻断输入的兜底时长。没有起身 Montage 时也会用它短暂保护状态切换。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|GetUp", meta = (ClampMin = "0.0"))
	float FallbackGetUpBlockDuration = 0.6f;

	/** 骨盆骨骼名。UE Mannequin 通常是 pelvis。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll")
	FName PelvisBoneName = TEXT("pelvis");

	/** Mesh 进入布娃娃时使用的碰撞配置，通常保持 Ragdoll。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll")
	FName RagdollCollisionProfileName = TEXT("Ragdoll");

	/** 地面检测使用的 Trace Channel。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|Ragdoll")
	TEnumAsByte<ECollisionChannel> RagdollGroundTraceChannel = ECC_Visibility;

	/** 如果前后起身判断反了，先勾选这个，不急着改代码。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|GetUp")
	bool bInvertGetUpDirection = false;

	/** 面朝地面时播放的起身 Montage。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|GetUp")
	TObjectPtr<UAnimMontage> GetUpFromFrontMontage;

	/** 背朝地面时播放的起身 Montage。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|GetUp")
	TObjectPtr<UAnimMontage> GetUpFromBackMontage;

	/** 起身 Montage 播放速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|HitPhysics|GetUp", meta = (ClampMin = "0.1"))
	float GetUpPlayRate = 1.0f;

private:
	float LastHitFlyTime = -1.0f;
	float RagdollStartTime = -1.0f;

	bool bIsRagdollPending = false;
	bool bIsRagdollActive = false;
	bool bIsGettingUp = false;

	FVector PendingRagdollImpulse = FVector::ZeroVector;
	FTransform InitialMeshRelativeTransform = FTransform::Identity;
	bool bHasCachedMeshTransform = false;

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
	EActionRagdollGetUpType DetermineGetUpType() const;
	UAnimMontage* GetGetUpMontage(EActionRagdollGetUpType GetUpType) const;

	void AddRagdollBlockTags();
	void ClearRagdollBlockTags();
	void CacheDefaultMeshTransformAndCollision();
	void SaveCurrentCollisionState();
	void RestoreCollisionState();
	void RestoreCharacterFromRagdoll(const FHitResult& GroundHit);
};

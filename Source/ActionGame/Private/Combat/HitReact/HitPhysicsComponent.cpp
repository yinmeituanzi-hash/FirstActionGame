#include "Combat/HitReact/HitPhysicsComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Char/ActionCharacterBase.h"
#include "CollisionQueryParams.h"
#include "Common/ActionGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHitPhysics, Log, All);

UHitPhysicsComponent::UHitPhysicsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHitPhysicsComponent::BeginPlay()
{
	Super::BeginPlay();
	CacheDefaultMeshTransformAndCollision();
}

void UHitPhysicsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsRagdollActive)
	{
		return;
	}

	const AActionCharacterBase* OwnerChar = GetOwnerCharacter();
	if (OwnerChar == nullptr || OwnerChar->IsDead())
	{
		return;
	}

	FHitResult GroundHit;
	if (IsRagdollReadyForGetUp(GroundHit))
	{
		FinishRagdollAndStartGetUp();
	}
}

bool UHitPhysicsComponent::CanApplyHitFly() const
{
	const AActionCharacterBase* OwnerChar = GetOwnerCharacter();
	if (OwnerChar == nullptr || OwnerChar->IsDead())
	{
		return false;
	}

	if (bIsRagdollPending || bIsRagdollActive || bIsGettingUp)
	{
		return false;
	}

	if (OwnerChar->HasActionTag(ActionGameplayTags::State_Ragdoll))
	{
		return false;
	}

	return GetRemainingHitFlyCooldown() <= 0.0f;
}

void UHitPhysicsComponent::ApplyHitImpulse(const FVector& HitDirection, float XYStrength, float ZStrength)
{
	AActionCharacterBase* OwnerChar = GetOwnerCharacter();
	UCharacterMovementComponent* Movement = GetOwnerMovement();
	if (OwnerChar == nullptr || Movement == nullptr)
	{
		return;
	}

	if (!CanApplyHitFly())
	{
		UE_LOG(
			LogHitPhysics,
			Verbose,
			TEXT("HitPhysics: HitFly skipped by cooldown or invalid state. Owner=%s RemainingCooldown=%.2f"),
			*GetNameSafe(OwnerChar),
			GetRemainingHitFlyCooldown());
		return;
	}

	FVector LaunchDirection = HitDirection.GetSafeNormal2D();
	if (LaunchDirection.IsNearlyZero())
	{
		LaunchDirection = OwnerChar->GetActorForwardVector().GetSafeNormal2D();
	}

	const float SafeXYStrength = FMath::Max(XYStrength, 0.0f);
	const float SafeZStrength = FMath::Max(ZStrength, 0.0f);
	const FVector LaunchVelocity = LaunchDirection * SafeXYStrength + FVector::UpVector * SafeZStrength;
	if (LaunchVelocity.IsNearlyZero())
	{
		UE_LOG(LogHitPhysics, Verbose, TEXT("HitPhysics: HitFly skipped because launch velocity is zero. Owner=%s"), *GetNameSafe(OwnerChar));
		return;
	}

	LaunchCharacterInternal(OwnerChar, Movement, LaunchVelocity);
	LastHitFlyTime = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : LastHitFlyTime;

	UE_LOG(
		LogHitPhysics,
		Log,
		TEXT("HitPhysics: Applied HitFly. Owner=%s Velocity=%s Cooldown=%.2f"),
		*GetNameSafe(OwnerChar),
		*LaunchVelocity.ToString(),
		HitFlyCooldown);
}

void UHitPhysicsComponent::StartRagdoll(const FVector& InitialImpulse)
{
	AActionCharacterBase* OwnerChar = GetOwnerCharacter();
	if (OwnerChar == nullptr || OwnerChar->IsDead())
	{
		return;
	}

	if (!CanApplyHitFly())
	{
		UE_LOG(LogHitPhysics, Verbose, TEXT("HitPhysics: Ragdoll skipped by cooldown or invalid state. Owner=%s"), *GetNameSafe(OwnerChar));
		return;
	}

	UWorld* World = GetWorld();
	LastHitFlyTime = World != nullptr ? World->GetTimeSeconds() : LastHitFlyTime;
	PendingRagdollImpulse = InitialImpulse;
	bIsRagdollPending = true;
	AddRagdollBlockTags();

	if (RagdollStartDelay > 0.0f && World != nullptr)
	{
		World->GetTimerManager().SetTimer(EnterRagdollTimerHandle, this, &UHitPhysicsComponent::EnterRagdoll, RagdollStartDelay, false);
	}
	else
	{
		EnterRagdoll();
	}

	UE_LOG(
		LogHitPhysics,
		Log,
		TEXT("HitPhysics: Ragdoll requested. Owner=%s Impulse=%s Delay=%.2f"),
		*GetNameSafe(OwnerChar),
		*InitialImpulse.ToString(),
		RagdollStartDelay);
}

float UHitPhysicsComponent::GetRemainingHitFlyCooldown() const
{
	if (HitFlyCooldown <= 0.0f || LastHitFlyTime < 0.0f || GetWorld() == nullptr)
	{
		return 0.0f;
	}

	const float ElapsedTime = GetWorld()->GetTimeSeconds() - LastHitFlyTime;
	return FMath::Max(HitFlyCooldown - ElapsedTime, 0.0f);
}

AActionCharacterBase* UHitPhysicsComponent::GetOwnerCharacter() const
{
	return Cast<AActionCharacterBase>(GetOwner());
}

UCharacterMovementComponent* UHitPhysicsComponent::GetOwnerMovement() const
{
	const AActionCharacterBase* OwnerChar = GetOwnerCharacter();
	return OwnerChar != nullptr ? OwnerChar->GetCharacterMovement() : nullptr;
}

USkeletalMeshComponent* UHitPhysicsComponent::GetOwnerMesh() const
{
	const AActionCharacterBase* OwnerChar = GetOwnerCharacter();
	return OwnerChar != nullptr ? OwnerChar->GetMesh() : nullptr;
}

UCapsuleComponent* UHitPhysicsComponent::GetOwnerCapsule() const
{
	const AActionCharacterBase* OwnerChar = GetOwnerCharacter();
	return OwnerChar != nullptr ? OwnerChar->GetCapsuleComponent() : nullptr;
}

void UHitPhysicsComponent::LaunchCharacterInternal(AActionCharacterBase* OwnerChar, UCharacterMovementComponent* Movement, const FVector& LaunchVelocity)
{
	if (bRecoverMovementModeBeforeLaunch && Movement->MovementMode == MOVE_None)
	{
		// 没有 MovementMode 时，LaunchCharacter 会排队速度，但 CharacterMovement 不会正常推进位置。
		Movement->SetMovementMode(MOVE_Walking);
	}

	if (bOverrideCurrentVelocityOnHitFly)
	{
		Movement->StopMovementImmediately();
	}

	OwnerChar->LaunchCharacter(LaunchVelocity, true, true);
}

void UHitPhysicsComponent::EnterRagdoll()
{
	AActionCharacterBase* OwnerChar = GetOwnerCharacter();
	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	UCapsuleComponent* Capsule = GetOwnerCapsule();
	UCharacterMovementComponent* Movement = GetOwnerMovement();
	if (OwnerChar == nullptr || Mesh == nullptr || Capsule == nullptr || Movement == nullptr || OwnerChar->IsDead())
	{
		ClearRagdollBlockTags();
		bIsRagdollPending = false;
		return;
	}

	if (Mesh->GetPhysicsAsset() == nullptr)
	{
		// 没有 PhysicsAsset 时无法进入真正 Ragdoll，降级为普通击飞，避免测试流程完全失效。
		bIsRagdollPending = false;
		ClearRagdollBlockTags();
		LaunchCharacterInternal(OwnerChar, Movement, PendingRagdollImpulse);
		UE_LOG(LogHitPhysics, Warning, TEXT("HitPhysics: Ragdoll fallback to LaunchCharacter because PhysicsAsset is missing. Owner=%s"), *GetNameSafe(OwnerChar));
		return;
	}

	SaveCurrentCollisionState();

	if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.05f);
	}

	Movement->StopMovementImmediately();
	Movement->DisableMovement();

	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCollisionProfileName(RagdollCollisionProfileName);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetAllBodiesPhysicsBlendWeight(1.0f);
	Mesh->SetAllBodiesSimulatePhysics(true);
	Mesh->SetSimulatePhysics(true);
	Mesh->WakeAllRigidBodies();

	if (!PendingRagdollImpulse.IsNearlyZero())
	{
		Mesh->AddImpulse(PendingRagdollImpulse, PelvisBoneName, true);
	}

	bIsRagdollPending = false;
	bIsRagdollActive = true;
	RagdollStartTime = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;
	SetComponentTickEnabled(true);

	UE_LOG(LogHitPhysics, Log, TEXT("HitPhysics: Ragdoll started. Owner=%s"), *GetNameSafe(OwnerChar));
}

void UHitPhysicsComponent::FinishRagdollAndStartGetUp()
{
	FHitResult GroundHit;
	TraceGroundBelowPelvis(GroundHit);

	const EActionRagdollGetUpType GetUpType = DetermineGetUpType();
	RestoreCharacterFromRagdoll(GroundHit);

	bIsRagdollActive = false;
	bIsGettingUp = true;
	SetComponentTickEnabled(false);

	UAnimMontage* GetUpMontage = GetGetUpMontage(GetUpType);
	float GetUpDuration = FallbackGetUpBlockDuration;
	if (AActionCharacterBase* OwnerChar = GetOwnerCharacter())
	{
		if (GetUpMontage != nullptr)
		{
			const float PlayedLength = OwnerChar->PlayAnimMontage(GetUpMontage, GetUpPlayRate);
			if (PlayedLength > 0.0f)
			{
				GetUpDuration = PlayedLength;
			}
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			FinishGetUpTimerHandle,
			this,
			&UHitPhysicsComponent::FinishGetUp,
			FMath::Max(GetUpDuration, 0.0f),
			false);
	}
	else
	{
		FinishGetUp();
	}

	UE_LOG(
		LogHitPhysics,
		Log,
		TEXT("HitPhysics: Ragdoll finished, get up started. Type=%d Duration=%.2f"),
		static_cast<int32>(GetUpType),
		GetUpDuration);
}

void UHitPhysicsComponent::FinishGetUp()
{
	bIsGettingUp = false;
	ClearRagdollBlockTags();
	UE_LOG(LogHitPhysics, Log, TEXT("HitPhysics: GetUp finished. Owner=%s"), *GetNameSafe(GetOwner()));
}

bool UHitPhysicsComponent::IsRagdollReadyForGetUp(FHitResult& OutGroundHit) const
{
	if (GetWorld() == nullptr || RagdollStartTime < 0.0f)
	{
		return false;
	}

	const float RagdollElapsedTime = GetWorld()->GetTimeSeconds() - RagdollStartTime;
	if (RagdollElapsedTime < MinimumRagdollTime)
	{
		return false;
	}

	if (RagdollElapsedTime >= MaxRagdollTime)
	{
		TraceGroundBelowPelvis(OutGroundHit);
		return true;
	}

	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (Mesh == nullptr)
	{
		return false;
	}

	const bool bHasGround = TraceGroundBelowPelvis(OutGroundHit);
	const float PelvisSpeed = Mesh->GetPhysicsLinearVelocity(PelvisBoneName).Size();
	return bHasGround && PelvisSpeed <= RagdollSettleSpeed;
}

bool UHitPhysicsComponent::TraceGroundBelowPelvis(FHitResult& OutGroundHit) const
{
	const UWorld* World = GetWorld();
	const AActionCharacterBase* OwnerChar = GetOwnerCharacter();
	if (World == nullptr || OwnerChar == nullptr)
	{
		return false;
	}

	const FVector Start = GetPelvisWorldLocation();
	const FVector End = Start - FVector::UpVector * RagdollGroundTraceDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ActionRagdollGroundTrace), false, OwnerChar);
	return World->LineTraceSingleByChannel(OutGroundHit, Start, End, RagdollGroundTraceChannel, QueryParams);
}

FVector UHitPhysicsComponent::GetPelvisWorldLocation() const
{
	const USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (Mesh == nullptr)
	{
		const AActionCharacterBase* OwnerChar = GetOwnerCharacter();
		return OwnerChar != nullptr ? OwnerChar->GetActorLocation() : FVector::ZeroVector;
	}

	return Mesh->DoesSocketExist(PelvisBoneName) ? Mesh->GetSocketLocation(PelvisBoneName) : Mesh->GetComponentLocation();
}

EActionRagdollGetUpType UHitPhysicsComponent::DetermineGetUpType() const
{
	const USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (Mesh == nullptr)
	{
		return EActionRagdollGetUpType::Back;
	}

	const FTransform PelvisTransform = Mesh->DoesSocketExist(PelvisBoneName)
		? Mesh->GetSocketTransform(PelvisBoneName, RTS_World)
		: Mesh->GetComponentTransform();

	const float PelvisUpDot = FVector::DotProduct(PelvisTransform.GetUnitAxis(EAxis::Z), FVector::UpVector);
	const bool bFaceUp = bInvertGetUpDirection ? PelvisUpDot < 0.0f : PelvisUpDot >= 0.0f;
	return bFaceUp ? EActionRagdollGetUpType::Back : EActionRagdollGetUpType::Front;
}

UAnimMontage* UHitPhysicsComponent::GetGetUpMontage(EActionRagdollGetUpType GetUpType) const
{
	return GetUpType == EActionRagdollGetUpType::Front
		? GetUpFromFrontMontage
		: GetUpFromBackMontage;
}

void UHitPhysicsComponent::AddRagdollBlockTags()
{
	if (AActionCharacterBase* OwnerChar = GetOwnerCharacter())
	{
		OwnerChar->AddActionTagExternal(ActionGameplayTags::State_Ragdoll);
		OwnerChar->AddActionTagExternal(ActionGameplayTags::Block_Attack);
		OwnerChar->AddActionTagExternal(ActionGameplayTags::Block_Dodge);
		OwnerChar->AddActionTagExternal(ActionGameplayTags::Block_Move);
		OwnerChar->AddActionTagExternal(ActionGameplayTags::Block_HitReact);
	}
}

void UHitPhysicsComponent::ClearRagdollBlockTags()
{
	if (AActionCharacterBase* OwnerChar = GetOwnerCharacter())
	{
		OwnerChar->RemoveActionTagExternal(ActionGameplayTags::State_Ragdoll);
		OwnerChar->RemoveActionTagExternal(ActionGameplayTags::Block_Attack);
		OwnerChar->RemoveActionTagExternal(ActionGameplayTags::Block_Dodge);
		OwnerChar->RemoveActionTagExternal(ActionGameplayTags::Block_Move);
		OwnerChar->RemoveActionTagExternal(ActionGameplayTags::Block_HitReact);
	}
}

void UHitPhysicsComponent::CacheDefaultMeshTransformAndCollision()
{
	if (const USkeletalMeshComponent* Mesh = GetOwnerMesh())
	{
		InitialMeshRelativeTransform = Mesh->GetRelativeTransform();
		bHasCachedMeshTransform = true;
	}
}

void UHitPhysicsComponent::SaveCurrentCollisionState()
{
	if (const UCapsuleComponent* Capsule = GetOwnerCapsule())
	{
		SavedCapsuleCollisionEnabled = Capsule->GetCollisionEnabled();
		SavedCapsuleCollisionProfileName = Capsule->GetCollisionProfileName();
	}

	if (const USkeletalMeshComponent* Mesh = GetOwnerMesh())
	{
		SavedMeshCollisionEnabled = Mesh->GetCollisionEnabled();
		SavedMeshCollisionProfileName = Mesh->GetCollisionProfileName();
		bSavedMeshGenerateOverlapEvents = Mesh->GetGenerateOverlapEvents();
	}
}

void UHitPhysicsComponent::RestoreCollisionState()
{
	if (UCapsuleComponent* Capsule = GetOwnerCapsule())
	{
		if (SavedCapsuleCollisionProfileName != NAME_None)
		{
			Capsule->SetCollisionProfileName(SavedCapsuleCollisionProfileName);
		}
		Capsule->SetCollisionEnabled(SavedCapsuleCollisionEnabled);
	}

	if (USkeletalMeshComponent* Mesh = GetOwnerMesh())
	{
		if (SavedMeshCollisionProfileName != NAME_None)
		{
			Mesh->SetCollisionProfileName(SavedMeshCollisionProfileName);
		}
		Mesh->SetCollisionEnabled(SavedMeshCollisionEnabled);
		Mesh->SetGenerateOverlapEvents(bSavedMeshGenerateOverlapEvents);
	}
}

void UHitPhysicsComponent::RestoreCharacterFromRagdoll(const FHitResult& GroundHit)
{
	AActionCharacterBase* OwnerChar = GetOwnerCharacter();
	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	UCapsuleComponent* Capsule = GetOwnerCapsule();
	UCharacterMovementComponent* Movement = GetOwnerMovement();
	if (OwnerChar == nullptr || Mesh == nullptr || Capsule == nullptr || Movement == nullptr)
	{
		return;
	}

	const FVector PelvisLocation = GetPelvisWorldLocation();
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float TargetZ = GroundHit.bBlockingHit ? GroundHit.ImpactPoint.Z + CapsuleHalfHeight : OwnerChar->GetActorLocation().Z;
	const FVector TargetLocation(PelvisLocation.X, PelvisLocation.Y, TargetZ);

	OwnerChar->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);

	Mesh->SetAllBodiesSimulatePhysics(false);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetAllBodiesPhysicsBlendWeight(0.0f);
	Mesh->AttachToComponent(Capsule, FAttachmentTransformRules::KeepRelativeTransform);
	if (bHasCachedMeshTransform)
	{
		Mesh->SetRelativeTransform(InitialMeshRelativeTransform);
	}

	RestoreCollisionState();
	Movement->SetMovementMode(MOVE_Walking);
}

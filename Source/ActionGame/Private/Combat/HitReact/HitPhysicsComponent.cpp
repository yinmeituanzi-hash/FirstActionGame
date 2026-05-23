#include "Combat/HitReact/HitPhysicsComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Char/ActionCharacterBase.h"
#include "CollisionQueryParams.h"
#include "Common/ActionGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
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
	CacheDefaultMeshState();
	ValidateRagdollSetup();
}

void UHitPhysicsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActionCharacterBase* OwnerChar = GetOwnerCharacter();
	if (OwnerChar == nullptr || OwnerChar->IsDead())
	{
		return;
	}

	if (bIsRestoringMeshRelativeTransform)
	{
		TickMeshRelativeTransformRestore(DeltaTime);
	}

	if (!bIsRagdollActive)
	{
		return;
	}

	if (bDrawDebugRagdoll)
	{
		DrawDebugRagdoll();
	}

	if (bSyncCapsuleToPelvisDuringRagdoll)
	{
		SyncCapsuleLocationToPelvis();
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
	if (OwnerChar == nullptr || Movement == nullptr || !CanApplyHitFly())
	{
		return;
	}

	FVector LaunchDirection = HitDirection.GetSafeNormal2D();
	if (LaunchDirection.IsNearlyZero())
	{
		LaunchDirection = OwnerChar->GetActorForwardVector().GetSafeNormal2D();
	}

	const FVector LaunchVelocity =
		LaunchDirection * FMath::Max(XYStrength, 0.0f) +
		FVector::UpVector * FMath::Max(ZStrength, 0.0f);

	if (LaunchVelocity.IsNearlyZero())
	{
		return;
	}

	LaunchCharacterInternal(OwnerChar, Movement, LaunchVelocity);
	LastHitFlyTime = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : LastHitFlyTime;

	UE_LOG(LogHitPhysics, Log, TEXT("HitPhysics: Applied hit fly. Owner=%s Velocity=%s"),
		*GetNameSafe(OwnerChar),
		*LaunchVelocity.ToString());
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
		UE_LOG(LogHitPhysics, Verbose, TEXT("HitPhysics: Ragdoll skipped. Owner=%s"), *GetNameSafe(OwnerChar));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		LastHitFlyTime = World->GetTimeSeconds();
		PendingRagdollImpulse = InitialImpulse;
		bIsRagdollPending = true;
		AddRagdollBlockTags();

		if (RagdollStartDelay > 0.0f)
		{
			World->GetTimerManager().SetTimer(EnterRagdollTimerHandle, this, &UHitPhysicsComponent::EnterRagdoll, RagdollStartDelay, false);
		}
		else
		{
			EnterRagdoll();
		}
	}
	else
	{
		PendingRagdollImpulse = InitialImpulse;
		bIsRagdollPending = true;
		AddRagdollBlockTags();
		EnterRagdoll();
	}
}

void UHitPhysicsComponent::CancelGetUp()
{
	if (!bIsGettingUp)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FinishGetUpTimerHandle);
	}

	bIsGettingUp = false;
	ClearRagdollBlockTags();
	OnGetUpFinished.Broadcast();

	UE_LOG(LogHitPhysics, Log, TEXT("HitPhysics: GetUp cancelled. Owner=%s"), *GetNameSafe(GetOwner()));
}

float UHitPhysicsComponent::GetRemainingHitFlyCooldown() const
{
	if (HitFlyCooldown <= 0.0f || LastHitFlyTime < 0.0f || GetWorld() == nullptr)
	{
		return 0.0f;
	}

	return FMath::Max(HitFlyCooldown - (GetWorld()->GetTimeSeconds() - LastHitFlyTime), 0.0f);
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
	if (OwnerChar == nullptr || Movement == nullptr)
	{
		return;
	}

	if (bRecoverMovementModeBeforeLaunch && Movement->MovementMode == MOVE_None)
	{
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
		bIsRagdollPending = false;
		ClearRagdollBlockTags();
		return;
	}

	if (Mesh->GetPhysicsAsset() == nullptr)
	{
		bIsRagdollPending = false;
		ClearRagdollBlockTags();
		LaunchCharacterInternal(OwnerChar, Movement, PendingRagdollImpulse);
		UE_LOG(LogHitPhysics, Warning, TEXT("HitPhysics: Ragdoll fallback, PhysicsAsset is missing. Owner=%s"), *GetNameSafe(OwnerChar));
		return;
	}

	CacheDefaultMeshState();
	SaveCurrentCollisionState();
	LogMeshState(TEXT("EnterRagdoll.BeforePhysics"));

	if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.05f);
	}

	Movement->StopMovementImmediately();
	Movement->DisableMovement();
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Mesh->SetCollisionProfileName(RagdollCollisionProfileName);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetAllBodiesBelowSimulatePhysics(PelvisBoneName, true);
	Mesh->SetAllBodiesBelowPhysicsBlendWeight(PelvisBoneName, 1.0f, false, true);
	Mesh->WakeAllRigidBodies();

	if (!PendingRagdollImpulse.IsNearlyZero())
	{
		Mesh->AddImpulse(PendingRagdollImpulse, PelvisBoneName, true);
	}

	bIsRagdollPending = false;
	bIsRagdollActive = true;
	RagdollStartTime = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;
	SetComponentTickEnabled(true);

	LogMeshState(TEXT("EnterRagdoll.AfterPhysics"));
	UE_LOG(LogHitPhysics, Log, TEXT("HitPhysics: Ragdoll started. Owner=%s"), *GetNameSafe(OwnerChar));
}

void UHitPhysicsComponent::FinishRagdollAndStartGetUp()
{
	AActionCharacterBase* OwnerChar = GetOwnerCharacter();
	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	UCharacterMovementComponent* Movement = GetOwnerMovement();
	if (OwnerChar == nullptr || Mesh == nullptr || Movement == nullptr)
	{
		return;
	}

	SyncCapsuleLocationToPelvis();

	const EActionRagdollGetUpType GetUpType = DetermineGetUpType();
	const FRotator TargetRotation = ComputeCapsuleRotationFromNeckPelvis();

	LogMeshState(TEXT("FinishRagdoll.BeforeSnapshot"));
	UE_LOG(LogHitPhysics, Log, TEXT("HitPhysics: Broadcasting snapshot request. IsBound=%d"),
		OnRagdollSnapshotRequested.IsBound() ? 1 : 0);
	OnRagdollSnapshotRequested.Broadcast();

	Mesh->SetAllBodiesBelowSimulatePhysics(PelvisBoneName, false);
	Mesh->SetAllBodiesBelowPhysicsBlendWeight(PelvisBoneName, 0.0f, false, true);
	RestoreCollisionState();

	if (Mesh->GetAttachParent() == nullptr)
	{
		if (UCapsuleComponent* Capsule = GetOwnerCapsule())
		{
			Mesh->AttachToComponent(Capsule, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	OwnerChar->SetActorRotation(TargetRotation, ETeleportType::TeleportPhysics);
	Movement->SetMovementMode(MOVE_Walking);

	UAnimMontage* GetUpMontage = GetGetUpMontage(GetUpType);
	float GetUpDuration = FallbackGetUpBlockDuration;
	if (GetUpMontage != nullptr)
	{
		const float PlayedLength = OwnerChar->PlayAnimMontage(GetUpMontage, GetUpPlayRate);
		if (PlayedLength > 0.0f)
		{
			GetUpDuration = PlayedLength;
		}
	}

	const float ClampedBlendDuration = FMath::Min(GetUpBlendDuration, GetUpDuration);
	OnGetUpStarted.Broadcast(GetUpType, ClampedBlendDuration);
	StartMeshRelativeTransformRestore(ClampedBlendDuration);

	bIsRagdollActive = false;
	bIsGettingUp = true;
	SetComponentTickEnabled(bIsRestoringMeshRelativeTransform);

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

	LogMeshState(TEXT("FinishRagdoll.AfterGetUpStarted"));
	UE_LOG(LogHitPhysics, Log, TEXT("HitPhysics: GetUp started. Type=%d Duration=%.2f Blend=%.2f"),
		static_cast<int32>(GetUpType),
		GetUpDuration,
		ClampedBlendDuration);
}

void UHitPhysicsComponent::FinishGetUp()
{
	bIsGettingUp = false;
	bIsRestoringMeshRelativeTransform = false;
	SetComponentTickEnabled(false);
	ClearRagdollBlockTags();
	OnGetUpFinished.Broadcast();
	LogMeshState(TEXT("FinishGetUp"));
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

	const bool bHasGround = TraceGroundBelowPelvis(OutGroundHit);
	if (!bHasGround || !OutGroundHit.bBlockingHit)
	{
		return false;
	}

	const float PelvisGroundDistance = FMath::Abs(OutGroundHit.TraceStart.Z - OutGroundHit.ImpactPoint.Z);
	if (PelvisGroundDistance > RagdollGetUpGroundDistance)
	{
		return false;
	}

	if (RagdollElapsedTime >= MaxRagdollTime)
	{
		return true;
	}

	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (Mesh == nullptr)
	{
		return false;
	}

	const float PelvisLinearSpeed = Mesh->GetPhysicsLinearVelocity(PelvisBoneName).Size();
	const float PelvisAngularSpeed = Mesh->GetPhysicsAngularVelocityInDegrees(PelvisBoneName).Size();
	return PelvisLinearSpeed <= RagdollSettleSpeed && PelvisAngularSpeed <= RagdollSettleAngularSpeed;
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

FVector UHitPhysicsComponent::GetNeckWorldLocation() const
{
	const USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (Mesh == nullptr || !Mesh->DoesSocketExist(NeckBoneName))
	{
		return GetPelvisWorldLocation() + FVector(0.0f, 0.0f, 50.0f);
	}

	return Mesh->GetSocketLocation(NeckBoneName);
}

bool UHitPhysicsComponent::IsRagdollFaceUp() const
{
	const USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (Mesh == nullptr || !Mesh->DoesSocketExist(PelvisBoneName))
	{
		return true;
	}

	const FRotator PelvisRot = Mesh->GetSocketRotation(PelvisBoneName);
	const FVector PelvisRight = FRotationMatrix(PelvisRot).GetUnitAxis(EAxis::Y);
	return PelvisRight.Z > 0.0f;
}

EActionRagdollGetUpType UHitPhysicsComponent::DetermineGetUpType() const
{
	const bool bFaceUp = bInvertGetUpDirection ? !IsRagdollFaceUp() : IsRagdollFaceUp();
	return bFaceUp ? EActionRagdollGetUpType::Back : EActionRagdollGetUpType::Front;
}

UAnimMontage* UHitPhysicsComponent::GetGetUpMontage(EActionRagdollGetUpType GetUpType) const
{
	return GetUpType == EActionRagdollGetUpType::Front
		? GetUpFromFrontMontage
		: GetUpFromBackMontage;
}

FRotator UHitPhysicsComponent::ComputeCapsuleRotationFromNeckPelvis() const
{
	const AActionCharacterBase* OwnerChar = GetOwnerCharacter();
	if (OwnerChar == nullptr)
	{
		return FRotator::ZeroRotator;
	}

	FVector Direction = GetNeckWorldLocation() - GetPelvisWorldLocation();
	const bool bFaceUp = bInvertGetUpDirection ? !IsRagdollFaceUp() : IsRagdollFaceUp();
	if (!bFaceUp)
	{
		Direction *= -1.0f;
	}

	FVector Horizontal(Direction.X, Direction.Y, 0.0f);
	if (Horizontal.IsNearlyZero())
	{
		return OwnerChar->GetActorRotation();
	}

	Horizontal.Normalize();
	return Horizontal.Rotation();
}

void UHitPhysicsComponent::SyncCapsuleLocationToPelvis()
{
	AActionCharacterBase* OwnerChar = GetOwnerCharacter();
	UCapsuleComponent* Capsule = GetOwnerCapsule();
	if (OwnerChar == nullptr || Capsule == nullptr)
	{
		return;
	}

	const FVector PelvisLocation = GetPelvisWorldLocation();
	FVector TargetLocation = OwnerChar->GetActorLocation();
	TargetLocation.X = PelvisLocation.X;
	TargetLocation.Y = PelvisLocation.Y;

	FHitResult GroundHit;
	if (TraceGroundBelowPelvis(GroundHit) && GroundHit.bBlockingHit)
	{
		TargetLocation.Z = GroundHit.ImpactPoint.Z + Capsule->GetScaledCapsuleHalfHeight();
	}

	OwnerChar->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
}

void UHitPhysicsComponent::StartMeshRelativeTransformRestore(float Duration)
{
	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (Mesh == nullptr || !bHasCachedMeshState)
	{
		bIsRestoringMeshRelativeTransform = false;
		return;
	}

	MeshRelativeRestoreStart = Mesh->GetRelativeTransform();
	MeshRelativeRestoreElapsedTime = 0.0f;
	MeshRelativeRestoreDuration = FMath::Max(Duration, KINDA_SMALL_NUMBER);
	bIsRestoringMeshRelativeTransform = !MeshRelativeRestoreStart.Equals(DefaultMeshRelativeTransform, 0.1f);

	if (!bIsRestoringMeshRelativeTransform)
	{
		return;
	}

	UE_LOG(LogHitPhysics, Log, TEXT("HitPhysics: Mesh relative transform restore started. Duration=%.2f From=%s To=%s"),
		MeshRelativeRestoreDuration,
		*MeshRelativeRestoreStart.ToString(),
		*DefaultMeshRelativeTransform.ToString());
}

void UHitPhysicsComponent::TickMeshRelativeTransformRestore(float DeltaTime)
{
	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (Mesh == nullptr || !bIsRestoringMeshRelativeTransform)
	{
		bIsRestoringMeshRelativeTransform = false;
		return;
	}

	MeshRelativeRestoreElapsedTime += DeltaTime;
	const float Alpha = FMath::Clamp(MeshRelativeRestoreElapsedTime / MeshRelativeRestoreDuration, 0.0f, 1.0f);
	const float SmoothAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

	const FVector Location = FMath::Lerp(
		MeshRelativeRestoreStart.GetLocation(),
		DefaultMeshRelativeTransform.GetLocation(),
		SmoothAlpha);
	const FQuat Rotation = FQuat::Slerp(
		MeshRelativeRestoreStart.GetRotation(),
		DefaultMeshRelativeTransform.GetRotation(),
		SmoothAlpha).GetNormalized();
	const FVector Scale = FMath::Lerp(
		MeshRelativeRestoreStart.GetScale3D(),
		DefaultMeshRelativeTransform.GetScale3D(),
		SmoothAlpha);

	Mesh->SetRelativeTransform(FTransform(Rotation, Location, Scale), false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.0f)
	{
		Mesh->SetRelativeTransform(DefaultMeshRelativeTransform, false, nullptr, ETeleportType::TeleportPhysics);
		bIsRestoringMeshRelativeTransform = false;

		if (!bIsRagdollActive)
		{
			SetComponentTickEnabled(false);
		}

		LogMeshState(TEXT("MeshRelativeRestore.Finished"));
	}
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
		// Day 6: Ragdoll 期间冻结 BT。AlertStateTick Service 0.2s 内同步到 BB.IsBlocked，
		// BT 根 Decorator 看到 IsBlocked=true 整树跳过；起身完成 ClearRagdollBlockTags 后自然恢复。
		OwnerChar->AddActionTagExternal(ActionGameplayTags::Block_AIControl);
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
		OwnerChar->RemoveActionTagExternal(ActionGameplayTags::Block_AIControl);
	}
}

void UHitPhysicsComponent::CacheDefaultMeshState()
{
	if (const USkeletalMeshComponent* Mesh = GetOwnerMesh())
	{
		DefaultMeshRelativeTransform = Mesh->GetRelativeTransform();
		DefaultMeshRelativeLocation = Mesh->GetRelativeLocation();
		DefaultMeshAttachParent = Mesh->GetAttachParent();
		DefaultMeshAttachSocketName = Mesh->GetAttachSocketName();
		bHasCachedMeshState = true;
	}
}

void UHitPhysicsComponent::ValidateRagdollSetup() const
{
	const USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (Mesh == nullptr)
	{
		return;
	}

	if (PelvisBoneName != NAME_None && !Mesh->DoesSocketExist(PelvisBoneName))
	{
		UE_LOG(LogHitPhysics, Warning, TEXT("HitPhysics: Pelvis bone/socket '%s' is missing. Owner=%s"),
			*PelvisBoneName.ToString(),
			*GetNameSafe(GetOwner()));
	}

	if (Mesh->GetPhysicsAsset() == nullptr)
	{
		UE_LOG(LogHitPhysics, Warning, TEXT("HitPhysics: PhysicsAsset is missing. Owner=%s"), *GetNameSafe(GetOwner()));
	}
}

void UHitPhysicsComponent::DrawDebugRagdoll() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (World == nullptr || Mesh == nullptr)
	{
		return;
	}

	const FVector PelvisLoc = GetPelvisWorldLocation();
	const FVector NeckLoc = GetNeckWorldLocation();
	DrawDebugLine(World, PelvisLoc, NeckLoc, FColor::Purple, false, DebugDrawLifetime, 0, 3.0f);
	DrawDebugLine(World, PelvisLoc, PelvisLoc - FVector::UpVector * RagdollGroundTraceDistance, FColor::Yellow, false, DebugDrawLifetime, 0, 1.0f);

	FHitResult GroundHit;
	if (TraceGroundBelowPelvis(GroundHit) && GroundHit.bBlockingHit)
	{
		DrawDebugCrosshairs(World, GroundHit.ImpactPoint, FRotator::ZeroRotator, 20.0f, FColor::Magenta, false, DebugDrawLifetime, 0);
	}

	const FString StatusText = FString::Printf(
		TEXT("Ragdoll Lin=%.0f Ang=%.0f"),
		Mesh->GetPhysicsLinearVelocity(PelvisBoneName).Size(),
		Mesh->GetPhysicsAngularVelocityInDegrees(PelvisBoneName).Size());
	DrawDebugString(World, PelvisLoc + FVector(0.0f, 0.0f, 80.0f), StatusText, nullptr, FColor::White, 0.0f, true);
#endif
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

void UHitPhysicsComponent::LogMeshState(const TCHAR* Phase) const
{
	const USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (Mesh == nullptr)
	{
		return;
	}

	const USceneComponent* CurrentParent = Mesh->GetAttachParent();
	const FName CurrentSocket = Mesh->GetAttachSocketName();
	const FTransform CurrentRelative = Mesh->GetRelativeTransform();
	const bool bParentChanged = bHasCachedMeshState && CurrentParent != DefaultMeshAttachParent.Get();
	const bool bSocketChanged = bHasCachedMeshState && CurrentSocket != DefaultMeshAttachSocketName;
	const bool bRelativeChanged = bHasCachedMeshState && !CurrentRelative.Equals(DefaultMeshRelativeTransform, 0.1f);

	if (bParentChanged || bSocketChanged || bRelativeChanged)
	{
		UE_LOG(LogHitPhysics, Warning,
			TEXT("HitPhysics MeshState[%s]: Parent=%s Socket=%s Rel=%s DefaultParent=%s DefaultSocket=%s DefaultRel=%s"),
			Phase,
			*GetNameSafe(CurrentParent),
			*CurrentSocket.ToString(),
			*CurrentRelative.ToString(),
			*GetNameSafe(DefaultMeshAttachParent.Get()),
			*DefaultMeshAttachSocketName.ToString(),
			*DefaultMeshRelativeTransform.ToString());
	}
	else
	{
		UE_LOG(LogHitPhysics, Verbose,
			TEXT("HitPhysics MeshState[%s]: Parent=%s Socket=%s Rel=%s"),
			Phase,
			*GetNameSafe(CurrentParent),
			*CurrentSocket.ToString(),
			*CurrentRelative.ToString());
	}
}

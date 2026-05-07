#include "Combat/LockOn/LockOnComponent.h"

#include "Camera/CameraComponent.h"
#include "Char/ActionPlayerCharacter.h"
#include "Combat/LockOn/ActionLockableInterface.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogLockOn, Log, All);

ULockOnComponent::ULockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULockOnComponent::BeginPlay()
{
	Super::BeginPlay();
}

void ULockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsLocked())
	{
		return;
	}

	// 1. 检查目标是否仍可锁定（死亡 / 超距离 / 接口拒绝）。
	if (!IsTargetStillValid(CurrentTarget.Get()))
	{
		UE_LOG(LogLockOn, Log, TEXT("LockOn: Target became invalid, auto unlocking."));
		RequestUnlock();
		return;
	}

	// 2. 把 ControlRotation 朝向目标插值。
	UpdateCameraTowardsTarget(DeltaTime);
}

// ============================================================================
// Public API
// ============================================================================

void ULockOnComponent::ToggleLockOn()
{
	if (IsLocked())
	{
		RequestUnlock();
	}
	else
	{
		RequestLockOn();
	}
}

void ULockOnComponent::RequestLockOn()
{
	if (IsLocked())
	{
		return;
	}

	TArray<AActor*> Candidates;
	CollectCandidates(Candidates);

	AActor* Best = PickBestTarget(Candidates);
	if (Best == nullptr)
	{
		UE_LOG(LogLockOn, Log, TEXT("LockOn: No valid target in range."));
		return;
	}

	SetCurrentTarget(Best);
}

void ULockOnComponent::RequestUnlock()
{
	if (!IsLocked())
	{
		return;
	}
	SetCurrentTarget(nullptr);
}

void ULockOnComponent::NotifyLookInput(float LookX)
{
	if (!IsLocked() || FMath::Abs(LookX) < SwitchTargetThreshold)
	{
		return;
	}

	const float Now = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now - LastSwitchTime < SwitchTargetCooldown)
	{
		return;
	}
	LastSwitchTime = Now;

	SwitchTarget(LookX > 0.0f);
}

// ============================================================================
// Internal
// ============================================================================

AActionPlayerCharacter* ULockOnComponent::GetPlayerOwner() const
{
	return Cast<AActionPlayerCharacter>(GetOwner());
}

APlayerController* ULockOnComponent::GetPlayerController() const
{
	if (AActionPlayerCharacter* Owner = GetPlayerOwner())
	{
		return Cast<APlayerController>(Owner->GetController());
	}
	return nullptr;
}

UCameraComponent* ULockOnComponent::GetFollowCamera() const
{
	if (AActionPlayerCharacter* Owner = GetPlayerOwner())
	{
		return Owner->GetFollowCamera();
	}
	return nullptr;
}

void ULockOnComponent::CollectCandidates(TArray<AActor*>& OutCandidates) const
{
	OutCandidates.Reset();

	const AActionPlayerCharacter* Owner = GetPlayerOwner();
	UWorld* World = GetWorld();
	if (Owner == nullptr || World == nullptr)
	{
		return;
	}

	const FVector OwnerLoc = Owner->GetActorLocation();
	const float RadiusSq = LockOnRadius * LockOnRadius;

	// 遍历所有实现了 IActionLockableInterface 的 Actor。
	// 角色数量级不大（动作游戏一屏几个怪），TActorIterator 性能足够。
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == nullptr || Actor == Owner)
		{
			continue;
		}
		if (!Actor->GetClass()->ImplementsInterface(UActionLockableInterface::StaticClass()))
		{
			continue;
		}
		if (!IActionLockableInterface::Execute_CanBeLockedOn(Actor))
		{
			continue;
		}
		if (FVector::DistSquared(OwnerLoc, Actor->GetActorLocation()) > RadiusSq)
		{
			continue;
		}

		OutCandidates.Add(Actor);
	}
}

AActor* ULockOnComponent::PickBestTarget(const TArray<AActor*>& Candidates, AActor* Excluding) const
{
	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();

	for (AActor* Candidate : Candidates)
	{
		if (Candidate == nullptr || Candidate == Excluding)
		{
			continue;
		}

		// 初次锁定要求目标在屏幕内（至少在前方 180° 锥内）。
		if (bRequireInScreenForInitialLock && !IsLocked())
		{
			const AActionPlayerCharacter* Owner = GetPlayerOwner();
			const APlayerController* PC = GetPlayerController();
			if (Owner != nullptr && PC != nullptr)
			{
				const FVector ToTarget = (Candidate->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
				const FVector CamForward = PC->GetControlRotation().Vector().GetSafeNormal2D();
				if (FVector::DotProduct(ToTarget, CamForward) <= 0.0f)
				{
					continue;
				}
			}
		}

		const float Score = ScoreCandidate(Candidate);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	return Best;
}

float ULockOnComponent::ScoreCandidate(AActor* Candidate) const
{
	const AActionPlayerCharacter* Owner = GetPlayerOwner();
	const APlayerController* PC = GetPlayerController();
	if (Owner == nullptr || PC == nullptr || Candidate == nullptr)
	{
		return TNumericLimits<float>::Max();
	}

	// 分量 1：世界距离（归一化到 [0, 1]）。
	const float WorldDist = FVector::Distance(Owner->GetActorLocation(), Candidate->GetActorLocation());
	const float DistScore = FMath::Clamp(WorldDist / FMath::Max(LockOnRadius, 1.0f), 0.0f, 1.0f);

	// 分量 2：屏幕中心距离（用"目标方向 与 相机方向"的角度来近似，避免做 ProjectWorldToScreen）。
	const FVector ToTarget = (Candidate->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal();
	const FVector CamForward = PC->GetControlRotation().Vector();
	const float Dot = FVector::DotProduct(ToTarget, CamForward);
	// Dot 范围 [-1, 1]，越大越靠近屏幕中心。把它变成"分数"（越小越好）：
	const float CenterScore = (1.0f - Dot) * 0.5f;  // → [0, 1]

	// 加权求和：权重越高，越偏向"屏幕中心"。
	return DistScore + CenterScore * ScreenCenterWeight;
}

bool ULockOnComponent::IsTargetStillValid(AActor* Target) const
{
	if (Target == nullptr || !IsValid(Target))
	{
		return false;
	}
	if (!Target->GetClass()->ImplementsInterface(UActionLockableInterface::StaticClass()))
	{
		return false;
	}
	if (!IActionLockableInterface::Execute_CanBeLockedOn(Target))
	{
		return false;
	}

	const AActionPlayerCharacter* Owner = GetPlayerOwner();
	if (Owner == nullptr)
	{
		return false;
	}
	if (FVector::Distance(Owner->GetActorLocation(), Target->GetActorLocation()) > UnlockDistance)
	{
		return false;
	}

	return true;
}

void ULockOnComponent::SetCurrentTarget(AActor* NewTarget)
{
	AActor* OldTarget = CurrentTarget.Get();
	if (OldTarget == NewTarget)
	{
		return;
	}

	// 旧目标 → 通知解锁。
	if (OldTarget != nullptr && OldTarget->GetClass()->ImplementsInterface(UActionLockableInterface::StaticClass()))
	{
		IActionLockableInterface::Execute_OnLockedOff(OldTarget);
	}

	CurrentTarget = NewTarget;

	// 新目标 → 通知锁定 + 调整运动模式。
	const bool bNowLocked = (NewTarget != nullptr);
	if (NewTarget != nullptr && NewTarget->GetClass()->ImplementsInterface(UActionLockableInterface::StaticClass()))
	{
		IActionLockableInterface::Execute_OnLockedOn(NewTarget);
	}

	// 第一次进入锁定 / 完全离开锁定 → 切换运动模式。
	const bool bWasLocked = (OldTarget != nullptr);
	if (bNowLocked != bWasLocked)
	{
		ApplyMovementModeForLockState(bNowLocked);
	}

	OnLockOnTargetChanged.Broadcast(NewTarget);
	UE_LOG(LogLockOn, Log, TEXT("LockOn: Target changed to %s."), *GetNameSafe(NewTarget));
}

void ULockOnComponent::ApplyMovementModeForLockState(bool bLocked)
{
	AActionPlayerCharacter* Owner = GetPlayerOwner();
	if (Owner == nullptr)
	{
		return;
	}

	UCharacterMovementComponent* Movement = Owner->GetCharacterMovement();
	if (Movement == nullptr)
	{
		return;
	}

	if (bLocked)
	{
		// 缓存原配置，进入"第三人称射击"模式：角色面朝相机方向，可以侧步。
		bCachedOrientToMovement = Movement->bOrientRotationToMovement;
		bCachedUseControllerYaw = Owner->bUseControllerRotationYaw;

		Movement->bOrientRotationToMovement = false;
		Owner->bUseControllerRotationYaw = true;
	}
	else
	{
		// 还原。
		Movement->bOrientRotationToMovement = bCachedOrientToMovement;
		Owner->bUseControllerRotationYaw = bCachedUseControllerYaw;
	}
}

void ULockOnComponent::UpdateCameraTowardsTarget(float DeltaTime)
{
	APlayerController* PC = GetPlayerController();
	AActionPlayerCharacter* Owner = GetPlayerOwner();
	AActor* Target = CurrentTarget.Get();
	if (PC == nullptr || Owner == nullptr || Target == nullptr)
	{
		return;
	}

	// 目标位置：优先用接口提供的（可能是胸口 socket），否则用 Actor 位置。
	FVector TargetLoc = Target->GetActorLocation();
	if (Target->GetClass()->ImplementsInterface(UActionLockableInterface::StaticClass()))
	{
		const FVector InterfaceLoc = IActionLockableInterface::Execute_GetLockOnTargetLocation(Target);
		if (!InterfaceLoc.IsZero())
		{
			TargetLoc = InterfaceLoc;
		}
	}

	// 相机看的是"FollowCamera 位置 → TargetLoc 的方向"。
	UCameraComponent* Camera = GetFollowCamera();
	const FVector CameraLoc = Camera != nullptr ? Camera->GetComponentLocation() : Owner->GetActorLocation();
	const FRotator DesiredRot = (TargetLoc - CameraLoc).Rotation();

	// 加 Pitch 偏移，让目标处于屏幕中上方更舒服。
	FRotator FinalRot = DesiredRot;
	FinalRot.Pitch = FMath::Clamp(DesiredRot.Pitch + CameraPitchOffset, -75.0f, 75.0f);
	FinalRot.Roll = 0.0f;

	const FRotator CurrentRot = PC->GetControlRotation();
	const FRotator NewRot = FMath::RInterpTo(CurrentRot, FinalRot, DeltaTime, CameraInterpSpeed);
	PC->SetControlRotation(NewRot);
}

void ULockOnComponent::SwitchTarget(bool bRight)
{
	AActor* OldTarget = CurrentTarget.Get();
	if (OldTarget == nullptr)
	{
		return;
	}

	AActionPlayerCharacter* Owner = GetPlayerOwner();
	APlayerController* PC = GetPlayerController();
	if (Owner == nullptr || PC == nullptr)
	{
		return;
	}

	TArray<AActor*> Candidates;
	CollectCandidates(Candidates);

	const FVector OwnerLoc = Owner->GetActorLocation();
	const FVector OldDir = (OldTarget->GetActorLocation() - OwnerLoc).GetSafeNormal2D();
	const FVector CamForward = PC->GetControlRotation().Vector().GetSafeNormal2D();

	// 在水平面上：相机右手向量。
	const FVector CamRight = FVector::CrossProduct(FVector::UpVector, CamForward).GetSafeNormal2D();

	AActor* Best = nullptr;
	float BestAngle = TNumericLimits<float>::Max();

	for (AActor* Candidate : Candidates)
	{
		if (Candidate == nullptr || Candidate == OldTarget)
		{
			continue;
		}
		const FVector NewDir = (Candidate->GetActorLocation() - OwnerLoc).GetSafeNormal2D();

		// 用旧目标 → 候选目标 的"侧向投影"判断左右：
		const FVector DiffDir = (NewDir - OldDir).GetSafeNormal2D();
		const float SideDot = FVector::DotProduct(DiffDir, CamRight);
		const bool bIsRightSide = SideDot > 0.0f;
		if (bIsRightSide != bRight)
		{
			continue;
		}

		// 在同一侧的候选里选"角度差最小"的（也就是最邻近的）。
		const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(NewDir, OldDir), -1.0f, 1.0f));
		if (Angle < BestAngle)
		{
			BestAngle = Angle;
			Best = Candidate;
		}
	}

	if (Best != nullptr)
	{
		SetCurrentTarget(Best);
	}
}

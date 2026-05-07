#include "Combat/ActionFeatures/ActionFeatureBase.h"
#include "Animation/AnimInstance.h"
#include "Char/ActionPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionFeature, Log, All);

UActionFeatureBase::UActionFeatureBase()
{
}

void UActionFeatureBase::Initialize(AActionPlayerCharacter* InOwner)
{
	OwnerChar = InOwner;
	LastExecuteTime = -1.0f;
	bIsActive = false;
}

bool UActionFeatureBase::CanExecute() const
{
	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner == nullptr)
	{
		return false;
	}

	if (Owner->IsDead())
	{
		return false;
	}

	if (Owner->HasAnyActionTags(BlockTags))
	{
		return false;
	}

	if (IsInCooldown())
	{
		return false;
	}

	return true;
}

void UActionFeatureBase::Execute()
{
	// 子类必须实现具体逻辑，基类只做安全提示。
	UE_LOG(LogActionFeature, Warning, TEXT("UActionFeatureBase::Execute called on base class. Feature=%s"), *FeatureName.ToString());
}

void UActionFeatureBase::Stop(bool bInterrupted)
{
	EndActive(bInterrupted);
}

bool UActionFeatureBase::IsInCooldown() const
{
	if (CooldownTime <= 0.0f || LastExecuteTime < 0.0f)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	return (World->GetTimeSeconds() - LastExecuteTime) < CooldownTime;
}

void UActionFeatureBase::BeginActive()
{
	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner == nullptr)
	{
		return;
	}

	// 与已激活的其他 Feature 互斥。
	UActionFeatureBase* PreviousActive = Owner->GetCurrentActiveFeature();
	if (PreviousActive != nullptr && PreviousActive != this && PreviousActive->IsActive())
	{
		PreviousActive->Stop(true);
	}

	bIsActive = true;
	if (const UWorld* World = GetWorld())
	{
		LastExecuteTime = World->GetTimeSeconds();
	}

	Owner->SetCurrentActiveFeature(this);
	Owner->RequestActionState(TargetState);
}

void UActionFeatureBase::EndActive(bool bInterrupted)
{
	bIsActive = false;

	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner == nullptr)
	{
		return;
	}

	// 让 PlayerCharacter 帮我们决定回退状态（HitReact/Dead 优先于 Idle）。
	if (Owner->GetCurrentActiveFeature() == this)
	{
		Owner->ClearCurrentActiveFeature(this);
	}
}

UAnimInstance* UActionFeatureBase::GetOwnerAnimInstance() const
{
	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner == nullptr)
	{
		return nullptr;
	}

	USkeletalMeshComponent* Mesh = Owner->GetMesh();
	return Mesh != nullptr ? Mesh->GetAnimInstance() : nullptr;
}

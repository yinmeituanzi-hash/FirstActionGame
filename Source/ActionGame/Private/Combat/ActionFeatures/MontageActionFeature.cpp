#include "Combat/ActionFeatures/MontageActionFeature.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Char/ActionPlayerCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogMontageActionFeature, Log, All);

UMontageActionFeature::UMontageActionFeature()
{
}

void UMontageActionFeature::Stop(bool bInterrupted)
{
	StopActiveMontage();
	Super::Stop(bInterrupted);
}

float UMontageActionFeature::PlayMontageInternal(UAnimMontage* Montage, float PlayRate)
{
	if (Montage == nullptr)
	{
		UE_LOG(LogMontageActionFeature, Warning, TEXT("MontageActionFeature[%s]: PlayMontageInternal called with null montage."), *FeatureName.ToString());
		return 0.0f;
	}

	UAnimInstance* AnimInstance = GetOwnerAnimInstance();
	if (AnimInstance == nullptr)
	{
		UE_LOG(LogMontageActionFeature, Warning, TEXT("MontageActionFeature[%s]: AnimInstance is not available."), *FeatureName.ToString());
		return 0.0f;
	}

	// 如果当前还有 Active 蒙太奇且不同，先停掉。
	if (ActiveMontage != nullptr && ActiveMontage != Montage && AnimInstance->Montage_IsPlaying(ActiveMontage))
	{
		ClearMontageDelegates(AnimInstance);
		AnimInstance->Montage_Stop(DefaultMontageBlendOutTime, ActiveMontage);
	}

	const float Duration = AnimInstance->Montage_Play(Montage, PlayRate);
	if (Duration <= 0.0f)
	{
		UE_LOG(LogMontageActionFeature, Warning, TEXT("MontageActionFeature[%s]: Montage_Play failed for %s."), *FeatureName.ToString(), *GetNameSafe(Montage));
		return 0.0f;
	}

	ActiveMontage = Montage;

	// 重新绑定 End 委托：每次都先 Unbind 再 Bind，避免重复触发。
	FOnMontageEnded EndedDelegate;
	EndedDelegate.BindUObject(this, &UMontageActionFeature::HandleMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndedDelegate, Montage);

	// Notify 委托使用 AddUniqueDynamic 防重复绑定。
	AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UMontageActionFeature::HandleMontageNotifyBegin);
	AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &UMontageActionFeature::HandleMontageNotifyBegin);

	return Duration;
}

void UMontageActionFeature::StopActiveMontage(float BlendOutTime)
{
	UAnimInstance* AnimInstance = GetOwnerAnimInstance();
	if (AnimInstance == nullptr || ActiveMontage == nullptr)
	{
		ActiveMontage = nullptr;
		return;
	}

	const float UseBlend = BlendOutTime < 0.0f ? DefaultMontageBlendOutTime : BlendOutTime;
	if (AnimInstance->Montage_IsPlaying(ActiveMontage))
	{
		ClearMontageDelegates(AnimInstance);
		AnimInstance->Montage_Stop(UseBlend, ActiveMontage);
	}

	ActiveMontage = nullptr;
}

void UMontageActionFeature::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// 仅处理由本 Feature 启动的蒙太奇结束。
	if (Montage != ActiveMontage)
	{
		return;
	}

	OnMontageEnded(Montage, bInterrupted);
	ActiveMontage = nullptr;
}

void UMontageActionFeature::HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
	// 当 PlayerCharacter 同时挂多个 Feature 委托时，
	// 每个 Feature 用 ActiveMontage 校验只处理自己的 Notify，避免串扰。
	if (ActiveMontage == nullptr)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetOwnerAnimInstance();
	if (AnimInstance == nullptr || !AnimInstance->Montage_IsPlaying(ActiveMontage))
	{
		return;
	}

	OnNotify(NotifyName, BranchingPointPayload);
}

void UMontageActionFeature::ClearMontageDelegates(UAnimInstance* AnimInstance)
{
	if (AnimInstance == nullptr)
	{
		return;
	}

	AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UMontageActionFeature::HandleMontageNotifyBegin);
}

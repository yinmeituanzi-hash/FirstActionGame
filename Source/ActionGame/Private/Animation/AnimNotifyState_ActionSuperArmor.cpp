#include "Animation/AnimNotifyState_ActionSuperArmor.h"
#include "Char/ActionCharacterBase.h"
#include "Common/ActionGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotifyState_ActionSuperArmor::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	SetSuperArmorTag(MeshComp, true);
}

void UAnimNotifyState_ActionSuperArmor::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	SetSuperArmorTag(MeshComp, false);
}

FString UAnimNotifyState_ActionSuperArmor::GetNotifyName_Implementation() const
{
	return TEXT("Action Super Armor");
}

void UAnimNotifyState_ActionSuperArmor::SetSuperArmorTag(USkeletalMeshComponent* MeshComp, bool bEnable)
{
	if (MeshComp == nullptr)
	{
		return;
	}

	AActionCharacterBase* Owner = Cast<AActionCharacterBase>(MeshComp->GetOwner());
	if (Owner == nullptr || Owner->IsDead())
	{
		return;
	}

	// Notify State 只负责维护霸体 Tag；是否跳过受击动画由 HitReceiver 统一判断。
	if (bEnable)
	{
		Owner->AddActionTagExternal(ActionGameplayTags::State_SuperArmor);
	}
	else
	{
		Owner->RemoveActionTagExternal(ActionGameplayTags::State_SuperArmor);
	}
}

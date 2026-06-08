#include "Animation/AnimNotifyState_ActionSkillCancelWindow.h"

#include "Char/ActionCharacterBase.h"
#include "Combat/Skills/ActionSkillComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotifyState_ActionSkillCancelWindow::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UActionSkillComponent* SkillComponent = GetSkillComponent(MeshComp))
	{
		SkillComponent->OpenSkillCancelWindow(
			CancelWindowMask,
			bReleaseMoveBlock,
			bReleaseDodgeBlock,
			bReleaseAttackBlock,
			bMarkRecoverWindow);
	}
}

void UAnimNotifyState_ActionSkillCancelWindow::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (UActionSkillComponent* SkillComponent = GetSkillComponent(MeshComp))
	{
		SkillComponent->CloseSkillCancelWindow(
			CancelWindowMask,
			bReleaseMoveBlock,
			bReleaseDodgeBlock,
			bReleaseAttackBlock,
			bMarkRecoverWindow);
	}
}

FString UAnimNotifyState_ActionSkillCancelWindow::GetNotifyName_Implementation() const
{
	return TEXT("SkillCancelWindow");
}

UActionSkillComponent* UAnimNotifyState_ActionSkillCancelWindow::GetSkillComponent(USkeletalMeshComponent* MeshComp)
{
	if (MeshComp == nullptr)
	{
		return nullptr;
	}

	AActionCharacterBase* OwnerCharacter = Cast<AActionCharacterBase>(MeshComp->GetOwner());
	return OwnerCharacter != nullptr ? OwnerCharacter->GetActionSkillComponent() : nullptr;
}

#include "Animation/AnimNotify_ActionQuitSkill.h"

#include "Char/ActionCharacterBase.h"
#include "Combat/Skills/ActionSkillComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_ActionQuitSkill::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp == nullptr)
	{
		return;
	}

	AActionCharacterBase* OwnerCharacter = Cast<AActionCharacterBase>(MeshComp->GetOwner());
	if (OwnerCharacter == nullptr)
	{
		return;
	}

	if (UActionSkillComponent* SkillComponent = OwnerCharacter->GetActionSkillComponent())
	{
		SkillComponent->OnQuitSkillNotify();
	}
}

FString UAnimNotify_ActionQuitSkill::GetNotifyName_Implementation() const
{
	return TEXT("ActionQuitSkill");
}

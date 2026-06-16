#include "Animation/AnimNotify_ActionSkillTurnWindow.h"

#include "Char/ActionCharacterBase.h"
#include "Combat/Skills/ActionSkillComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_ActionSkillTurnWindow::Notify(
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
		SkillComponent->OnTurnWindowNotify();
	}
}

FString UAnimNotify_ActionSkillTurnWindow::GetNotifyName_Implementation() const
{
	return TEXT("ActionSkillTurnWindow");
}

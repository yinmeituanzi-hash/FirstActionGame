#include "Animation/AnimNotify_ActionComboWindow.h"

#include "Char/ActionCharacterBase.h"
#include "Combat/Skills/ActionSkillComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_ActionComboWindow::Notify(
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
		SkillComponent->OnComboWindowNotify(ComboInputName, HoldType);
	}
}

FString UAnimNotify_ActionComboWindow::GetNotifyName_Implementation() const
{
	return ComboInputName.IsNone()
		? TEXT("ActionComboWindow")
		: FString::Printf(TEXT("Combo_%s"), *ComboInputName.ToString());
}

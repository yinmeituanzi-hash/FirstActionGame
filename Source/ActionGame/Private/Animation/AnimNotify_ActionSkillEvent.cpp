#include "Animation/AnimNotify_ActionSkillEvent.h"

#include "Char/ActionCharacterBase.h"
#include "Combat/Skills/ActionSkillComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_ActionSkillEvent::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp == nullptr || EventName.IsNone())
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
		SkillComponent->OnSkillNotify(EventName);
	}
}

FString UAnimNotify_ActionSkillEvent::GetNotifyName_Implementation() const
{
	return EventName.IsNone() ? TEXT("ActionSkillEvent") : FString::Printf(TEXT("SkillEvent_%s"), *EventName.ToString());
}

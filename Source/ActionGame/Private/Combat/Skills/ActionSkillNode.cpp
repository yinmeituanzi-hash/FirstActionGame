#include "Combat/Skills/ActionSkillNode.h"

#include "Char/ActionCharacterBase.h"
#include "Combat/Skills/ActionSkillComponent.h"
#include "Combat/Skills/ActionSkillEffectLibrary.h"
#include "Combat/Skills/ActionSkillObject.h"
#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionSkillNode, Log, All);

void UActionSkillNode::InitFromData(
	UActionSkillComponent* InOwnerComponent,
	UActionSkillObject* InSkillObject,
	FName InNodeId,
	const FActionSkillNodeRow& InNodeData,
	UDataTable* InSkillEffectDataTable)
{
	OwnerComponent = InOwnerComponent;
	SkillObject = InSkillObject;
	NodeId = InNodeId;
	NodeData = InNodeData;
	if (NodeData.NodeId.IsNone())
	{
		NodeData.NodeId = InNodeId;
	}
	SkillEffectDataTable = InSkillEffectDataTable;

	EffectsWhenEnter.Reset();
	EffectsWhenLeave.Reset();
	NotifyEffectMap.Reset();
	BuildEffectIndex(InSkillEffectDataTable);
}

void UActionSkillNode::Activate()
{
	bActive = true;
	if (const UActionSkillComponent* SkillComponent = OwnerComponent.Get())
	{
		if (const AActionCharacterBase* OwnerCharacter = SkillComponent->GetOwnerCharacter())
		{
			ActivationLocation = OwnerCharacter->GetActorLocation();
			ActivationRotation = OwnerCharacter->GetActorRotation();
		}
	}

	ExecuteEffects(EffectsWhenEnter, TEXT("Enter"));
}

void UActionSkillNode::Deactivate()
{
	if (!bActive)
	{
		return;
	}

	ExecuteEffects(EffectsWhenLeave, TEXT("Leave"));
	bActive = false;
}

void UActionSkillNode::OnNotify(FName EventName)
{
	if (EventName.IsNone())
	{
		return;
	}

	const TArray<FName>* EffectIds = NotifyEffectMap.Find(EventName);
	if (EffectIds == nullptr)
	{
		UE_LOG(
			LogActionSkillNode,
			Verbose,
			TEXT("SkillNode[%s]: Notify=%s has no mapped effects."),
			*NodeId.ToString(),
			*EventName.ToString());
		return;
	}

	ExecuteEffects(*EffectIds, FString::Printf(TEXT("Notify:%s"), *EventName.ToString()));
}

void UActionSkillNode::BuildEffectIndex(UDataTable* InSkillEffectDataTable)
{
	if (InSkillEffectDataTable == nullptr || InSkillEffectDataTable->GetRowStruct() != FActionSkillEffectRow::StaticStruct())
	{
		return;
	}

	for (const FName EffectId : NodeData.EffectIds)
	{
		if (EffectId.IsNone())
		{
			continue;
		}

		const FActionSkillEffectRow* EffectRow = InSkillEffectDataTable->FindRow<FActionSkillEffectRow>(EffectId, TEXT("ActionSkillNode.BuildEffectIndex"));
		if (EffectRow == nullptr)
		{
			UE_LOG(
				LogActionSkillNode,
				Warning,
				TEXT("SkillNode[%s]: Effect row not found. EffectId=%s"),
				*NodeId.ToString(),
				*EffectId.ToString());
			continue;
		}

		switch (EffectRow->ExecuteTiming)
		{
		case EActionSkillEffectTiming::Enter:
			EffectsWhenEnter.Add(EffectId);
			break;
		case EActionSkillEffectTiming::Leave:
			EffectsWhenLeave.Add(EffectId);
			break;
		case EActionSkillEffectTiming::Notify:
			if (!EffectRow->NotifyName.IsNone())
			{
				NotifyEffectMap.FindOrAdd(EffectRow->NotifyName).Add(EffectId);
			}
			break;
		default:
			break;
		}
	}
}

void UActionSkillNode::ExecuteEffects(const TArray<FName>& EffectIds, const FString& TimingText)
{
	if (EffectIds.Num() == 0)
	{
		return;
	}

	UActionSkillComponent* SkillComponent = OwnerComponent.Get();
	UActionSkillObject* Skill = SkillObject.Get();
	const FName SkillId = Skill != nullptr ? Skill->GetSkillId() : NAME_None;
	if (SkillEffectDataTable == nullptr || SkillEffectDataTable->GetRowStruct() != FActionSkillEffectRow::StaticStruct())
	{
		UE_LOG(
			LogActionSkillNode,
			Warning,
			TEXT("SkillNode[%s]: cannot execute effects because SkillEffectDataTable is invalid. Skill=%s Timing=%s"),
			*NodeId.ToString(),
			*SkillId.ToString(),
			*TimingText);
		return;
	}

	for (const FName EffectId : EffectIds)
	{
		const FActionSkillEffectRow* EffectRow = SkillEffectDataTable->FindRow<FActionSkillEffectRow>(EffectId, TEXT("ActionSkillNode.ExecuteEffects"));
		if (EffectRow == nullptr)
		{
			UE_LOG(
				LogActionSkillNode,
				Warning,
				TEXT("SkillNode[%s]: Effect row not found at execution. EffectId=%s Skill=%s Timing=%s"),
				*NodeId.ToString(),
				*EffectId.ToString(),
				*SkillId.ToString(),
				*TimingText);
			continue;
		}

		UE_LOG(
			LogActionSkillNode,
			Log,
			TEXT("SkillNode[%s]: Executing Effect=%s Skill=%s Timing=%s"),
			*NodeId.ToString(),
			*EffectId.ToString(),
			*SkillId.ToString(),
			*TimingText);

		UActionSkillEffectLibrary::ExecuteEffect(this, SkillComponent, Skill, this, EffectId, *EffectRow);
	}
}

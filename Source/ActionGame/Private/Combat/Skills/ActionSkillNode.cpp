#include "Combat/Skills/ActionSkillNode.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Char/ActionCharacterBase.h"
#include "Char/ActionCharacterMovementComponent.h"
#include "Combat/Skills/ActionSkillComponent.h"
#include "Combat/Skills/ActionSkillEffectLibrary.h"
#include "Combat/Skills/ActionSkillObject.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DataTable.h"
#include "Input/InputBufferComponent.h"

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
	ResetComboState();
	BuildEffectIndex(InSkillEffectDataTable);
}

void UActionSkillNode::Activate()
{
	bActive = true;
	ResetComboState();

	if (const UActionSkillComponent* SkillComponent = OwnerComponent.Get())
	{
		if (const AActionCharacterBase* OwnerCharacter = SkillComponent->GetOwnerCharacter())
		{
			ActivationLocation = OwnerCharacter->GetActorLocation();
			ActivationRotation = OwnerCharacter->GetActorRotation();
		}
	}

	ApplyRootMotionSettings();
	ExecuteEffects(EffectsWhenEnter, TEXT("Enter"));
}

void UActionSkillNode::Deactivate()
{
	if (bActive)
	{
		ExecuteEffects(EffectsWhenLeave, TEXT("Leave"));
		ClearRootMotionSettings();
		bActive = false;
	}

	ResetComboState();
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

void UActionSkillNode::OnNotifyNextCombo(FName InputName, FName HoldType)
{
	bCanEnterNextNode = true;
	ComboInputName = InputName;
	ComboHoldType = HoldType;
	LastMatchedComboInputName = NAME_None;

	UE_LOG(
		LogActionSkillNode,
		Verbose,
		TEXT("SkillNode[%s]: combo window opened. Input=%s Hold=%s Next=%s Branch=%s"),
		*NodeId.ToString(),
		*ComboInputName.ToString(),
		*ComboHoldType.ToString(),
		*NodeData.NextNodeId.ToString(),
		*NodeData.BranchNodeId.ToString());
}

void UActionSkillNode::OnNotifyQuitSkill()
{
	bQuitSkillFlag = true;
}

void UActionSkillNode::OnNotifyTurnWindow()
{
	bCanTurnNextNode = true;

	UE_LOG(
		LogActionSkillNode,
		Verbose,
		TEXT("SkillNode[%s]: turn window opened."),
		*NodeId.ToString());
}

FName UActionSkillNode::CheckComboTransition()
{
	LastMatchedComboInputName = NAME_None;

	if (!bCanEnterNextNode)
	{
		return NAME_None;
	}

	if (!NodeData.BranchNodeId.IsNone() && !ComboHoldType.IsNone() && HasValidBufferedInput(ComboHoldType))
	{
		LastMatchedComboInputName = ComboHoldType;
		return NodeData.BranchNodeId;
	}

	if (!NodeData.NextNodeId.IsNone() && HasValidBufferedInput(ComboInputName))
	{
		LastMatchedComboInputName = ComboInputName;
		return NodeData.NextNodeId;
	}

	// 没有显式 HoldType 时，BranchNodeId 作为 010 ExtraNextNodeId 风格的无输入备用节点。
	if (!NodeData.BranchNodeId.IsNone() && ComboHoldType.IsNone() && !HasValidBufferedInput(ComboInputName))
	{
		return NodeData.BranchNodeId;
	}

	return NAME_None;
}

void UActionSkillNode::TickByTimeLine()
{
	HandleMontageEndSkillQuit();
}

void UActionSkillNode::HandleMontageEndSkillQuit()
{
	if (!bActive || bQuitSkillFlag || NodeData.Montage == nullptr)
	{
		return;
	}

	const UActionSkillComponent* SkillComponent = OwnerComponent.Get();
	const AActionCharacterBase* OwnerCharacter = SkillComponent != nullptr ? SkillComponent->GetOwnerCharacter() : nullptr;
	UAnimInstance* AnimInstance = OwnerCharacter != nullptr && OwnerCharacter->GetMesh() != nullptr
		? OwnerCharacter->GetMesh()->GetAnimInstance()
		: nullptr;
	if (AnimInstance == nullptr)
	{
		return;
	}

	if (AnimInstance->Montage_IsPlaying(NodeData.Montage))
	{
		return;
	}

	bQuitSkillFlag = true;
	UE_LOG(
		LogActionSkillNode,
		Verbose,
		TEXT("SkillNode[%s]: montage stopped playing, mark QuitSkill. Montage=%s"),
		*NodeId.ToString(),
		*GetNameSafe(NodeData.Montage));
}

bool UActionSkillNode::HasValidInput() const
{
	return HasValidBufferedInput(ComboInputName)
		|| (!ComboHoldType.IsNone() && HasValidBufferedInput(ComboHoldType));
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

		const FActionSkillEffectRow* EffectRow = InSkillEffectDataTable->FindRow<FActionSkillEffectRow>(
			EffectId,
			TEXT("ActionSkillNode.BuildEffectIndex"));
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

	FActionSkillEffectContext Context;

	for (const FName EffectId : EffectIds)
	{
		const FActionSkillEffectRow* EffectRow = SkillEffectDataTable->FindRow<FActionSkillEffectRow>(
			EffectId,
			TEXT("ActionSkillNode.ExecuteEffects"));
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

		UActionSkillEffectLibrary::ExecuteEffect(this, SkillComponent, Skill, this, EffectId, *EffectRow, Context);
	}
}

void UActionSkillNode::ResetComboState()
{
	bCanEnterNextNode = false;
	bCanTurnNextNode = false;
	bQuitSkillFlag = false;
	ComboInputName = NAME_None;
	ComboHoldType = NAME_None;
	LastMatchedComboInputName = NAME_None;
}

bool UActionSkillNode::HasValidBufferedInput(FName InputName) const
{
	if (InputName.IsNone())
	{
		return false;
	}

	const UActionSkillComponent* SkillComponent = OwnerComponent.Get();
	const AActionCharacterBase* OwnerCharacter = SkillComponent != nullptr ? SkillComponent->GetOwnerCharacter() : nullptr;
	const UInputBufferComponent* InputBuffer = OwnerCharacter != nullptr
		? OwnerCharacter->FindComponentByClass<UInputBufferComponent>()
		: nullptr;

	return InputBuffer != nullptr && InputBuffer->HasValidInput(InputName);
}

void UActionSkillNode::ApplyRootMotionSettings()
{
	bAppliedRootMotionOverride = false;

	if (!NodeData.bUseRootMotion)
	{
		return;
	}

	const UActionSkillComponent* SkillComponent = OwnerComponent.Get();
	AActionCharacterBase* OwnerCharacter = SkillComponent != nullptr ? SkillComponent->GetOwnerCharacter() : nullptr;
	if (OwnerCharacter == nullptr)
	{
		return;
	}

	float EffectiveScale = NodeData.RootMotionScale;

	if (NodeData.RootMotionRadius > 0.0f)
	{
		const UActionSkillObject* Skill = SkillObject.Get();
		const AActor* Target = Skill != nullptr ? Skill->GetCurrentTarget() : nullptr;
		if (Target != nullptr)
		{
			const float Distance = FVector::Dist(OwnerCharacter->GetActorLocation(), Target->GetActorLocation());
			const float CapsuleRadius = OwnerCharacter->GetCapsuleComponent() != nullptr
				? OwnerCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius()
				: 0.0f;
			const float Threshold = CapsuleRadius * NodeData.RootMotionRadius;
			if (Distance > 0.0f && Distance <= Threshold)
			{
				EffectiveScale = 0.0f;
				UE_LOG(
					LogActionSkillNode,
					Verbose,
					TEXT("SkillNode[%s]: RootMotion disabled — distance %.1f <= threshold %.1f (radius=%.1f * capsule=%.1f)"),
					*NodeId.ToString(),
					Distance,
					Threshold,
					NodeData.RootMotionRadius,
					CapsuleRadius);
			}
		}
	}

	UActionCharacterMovementComponent* MovementComp = OwnerCharacter->GetActionCharacterMovement();
	if (MovementComp == nullptr)
	{
		return;
	}

	MovementComp->RootMotionZScale = EffectiveScale;
	bAppliedRootMotionOverride = true;

	UE_LOG(
		LogActionSkillNode,
		Verbose,
		TEXT("SkillNode[%s]: RootMotion applied. Scale=%.2f"),
		*NodeId.ToString(),
		EffectiveScale);
}

void UActionSkillNode::ClearRootMotionSettings()
{
	if (!bAppliedRootMotionOverride)
	{
		return;
	}

	bAppliedRootMotionOverride = false;

	const UActionSkillComponent* SkillComponent = OwnerComponent.Get();
	AActionCharacterBase* OwnerCharacter = SkillComponent != nullptr ? SkillComponent->GetOwnerCharacter() : nullptr;
	if (OwnerCharacter == nullptr)
	{
		return;
	}

	UActionCharacterMovementComponent* MovementComp = OwnerCharacter->GetActionCharacterMovement();
	if (MovementComp != nullptr)
	{
		MovementComp->RootMotionZScale = 1.0f;
	}
}

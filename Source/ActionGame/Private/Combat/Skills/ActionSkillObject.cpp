#include "Combat/Skills/ActionSkillObject.h"

#include "Char/ActionCharacterBase.h"
#include "Combat/Skills/ActionSkillNode.h"
#include "Engine/DataTable.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionSkillObject, Log, All);

void UActionSkillObject::InitFromData(AActionCharacterBase* InOwner, FName InSkillId, const FActionSkillRow& InSkillData)
{
	OwnerCharacter = InOwner;
	SkillId = InSkillId;
	SkillData = InSkillData;
	if (SkillData.SkillId.IsNone())
	{
		SkillData.SkillId = InSkillId;
	}
	CooldownRemaining = 0.0f;
	bActive = false;
	NodeMap.Reset();
	bInitialized = InOwner != nullptr && !SkillId.IsNone();
}

bool UActionSkillObject::CanActivate() const
{
	if (!bInitialized || bActive || CooldownRemaining > 0.0f || !OwnerCharacter.IsValid())
	{
		return false;
	}

	const AActionCharacterBase* Owner = OwnerCharacter.Get();
	if (Owner == nullptr || Owner->IsDead())
	{
		return false;
	}

	if (!SkillData.bAllowInAir)
	{
		const UCharacterMovementComponent* Movement = Owner->GetCharacterMovement();
		if (Movement != nullptr && Movement->IsFalling())
		{
			return false;
		}
	}

	return true;
}

void UActionSkillObject::Activate(AActor* InTarget)
{
	if (!CanActivate())
	{
		return;
	}

	bActive = true;
	CurrentTarget = InTarget;
	ResetHitActorsThisNode();
	NodeMap.Reset();
}

void UActionSkillObject::Deactivate(EActionSkillStopReason Reason)
{
	if (!bActive)
	{
		NodeMap.Reset();
		return;
	}

	bActive = false;
	CurrentTarget.Reset();
	ResetHitActorsThisNode();
	NodeMap.Reset();
	LastStopReason = Reason;

	if (Reason != EActionSkillStopReason::HitInterrupt || SkillData.bStartCooldownOnHitInterrupt)
	{
		CooldownRemaining = FMath::Max(0.0f, SkillData.Cooldown);
	}
}

void UActionSkillObject::TickCooldown(float DeltaTime)
{
	if (CooldownRemaining > 0.0f)
	{
		CooldownRemaining = FMath::Max(0.0f, CooldownRemaining - FMath::Max(0.0f, DeltaTime));
	}
}

bool UActionSkillObject::InitSkillNodes(
	UActionSkillComponent* OwnerComponent,
	UDataTable* SkillNodeDataTable,
	UDataTable* SkillEffectDataTable)
{
	NodeMap.Reset();

	if (SkillData.BeginNodeId.IsNone())
	{
		return true;
	}

	if (OwnerComponent == nullptr
		|| SkillNodeDataTable == nullptr
		|| SkillNodeDataTable->GetRowStruct() != FActionSkillNodeRow::StaticStruct())
	{
		UE_LOG(
			LogActionSkillObject,
			Warning,
			TEXT("SkillObject[%s]: cannot init nodes because node table is invalid."),
			*SkillId.ToString());
		return false;
	}

	return InitSkillNodeRecursive(SkillData.BeginNodeId, OwnerComponent, SkillNodeDataTable, SkillEffectDataTable);
}

UActionSkillNode* UActionSkillObject::GetSkillNode(FName NodeId) const
{
	if (const TObjectPtr<UActionSkillNode>* FoundNode = NodeMap.Find(NodeId))
	{
		return FoundNode->Get();
	}

	return nullptr;
}

bool UActionSkillObject::CanBeCancelledBy(EActionSkillCancelFlag IncomingType) const
{
	const int32 IncomingMask = static_cast<int32>(IncomingType);
	return IncomingMask != 0 && (SkillData.AllowCancelBy & IncomingMask) != 0;
}

void UActionSkillObject::ResetHitActorsThisNode()
{
	HitActorsThisNode.Reset();
}

bool UActionSkillObject::InitSkillNodeRecursive(
	FName NodeId,
	UActionSkillComponent* OwnerComponent,
	UDataTable* SkillNodeDataTable,
	UDataTable* SkillEffectDataTable)
{
	if (NodeId.IsNone())
	{
		return true;
	}

	if (NodeMap.Contains(NodeId))
	{
		return true;
	}

	const FActionSkillNodeRow* NodeRow = SkillNodeDataTable->FindRow<FActionSkillNodeRow>(
		NodeId,
		TEXT("ActionSkillObject.InitSkillNodeRecursive"));
	if (NodeRow == nullptr)
	{
		UE_LOG(
			LogActionSkillObject,
			Warning,
			TEXT("SkillObject[%s]: node row not found. NodeId=%s"),
			*SkillId.ToString(),
			*NodeId.ToString());
		return false;
	}

	UActionSkillNode* NewNode = NewObject<UActionSkillNode>(this);
	NewNode->InitFromData(OwnerComponent, this, NodeId, *NodeRow, SkillEffectDataTable);
	NodeMap.Add(NodeId, NewNode);

	const bool bNextOk = InitSkillNodeRecursive(NodeRow->NextNodeId, OwnerComponent, SkillNodeDataTable, SkillEffectDataTable);
	const bool bBranchOk = InitSkillNodeRecursive(NodeRow->BranchNodeId, OwnerComponent, SkillNodeDataTable, SkillEffectDataTable);
	return bNextOk && bBranchOk;
}

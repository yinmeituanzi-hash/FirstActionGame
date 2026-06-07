#include "Combat/Skills/ActionSkillComponent.h"

#include "Char/ActionCharacterBase.h"
#include "Combat/Skills/ActionSkillObject.h"
#include "Common/ActionGameplayTags.h"
#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionSkill, Log, All);

// 角色侧技能运行时组件。Day1 先处理数据加载、生命周期状态和冷却；
// 技能节点与效果执行会在后续 Day 逐步叠加。
UActionSkillComponent::UActionSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UActionSkillComponent::BeginPlay()
{
	Super::BeginPlay();

	LoadSkillObjectsFromTable();
}

void UActionSkillComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// SkillObject 是持久对象，所以即使技能当前没有激活，也能继续推进自己的冷却。
	// DataTable 行只作为配置来源，不承载这些运行时变化。
	for (TPair<FName, TObjectPtr<UActionSkillObject>>& Pair : SkillObjectMap)
	{
		if (Pair.Value != nullptr)
		{
			Pair.Value->TickCooldown(DeltaTime);
		}
	}
}

AActionCharacterBase* UActionSkillComponent::GetOwnerCharacter() const
{
	return Cast<AActionCharacterBase>(GetOwner());
}

FName UActionSkillComponent::GetCurrentSkillId() const
{
	return CurrentSkillObject != nullptr ? CurrentSkillObject->GetSkillId() : NAME_None;
}

UActionSkillObject* UActionSkillComponent::GetSkillObject(FName SkillId) const
{
	if (const TObjectPtr<UActionSkillObject>* Found = SkillObjectMap.Find(SkillId))
	{
		return Found->Get();
	}
	return nullptr;
}

float UActionSkillComponent::GetSkillCooldownRemaining(FName SkillId) const
{
	const UActionSkillObject* SkillObject = GetSkillObject(SkillId);
	return SkillObject != nullptr ? SkillObject->GetCooldownRemaining() : 0.0f;
}

bool UActionSkillComponent::CanUseSkill(FName SkillId, EActionSkillCancelFlag IncomingType) const
{
	const AActionCharacterBase* OwnerCharacter = GetOwnerCharacter();
	const UActionSkillObject* SkillObject = GetSkillObject(SkillId);
	if (OwnerCharacter == nullptr || OwnerCharacter->IsDead() || SkillObject == nullptr)
	{
		return false;
	}

	if (!SkillObject->CanActivate())
	{
		return false;
	}

	if (CurrentSkillObject != nullptr)
	{
		if (CurrentSkillObject == SkillObject)
		{
			return false;
		}

		if (!CurrentSkillObject->CanBeCancelledBy(IncomingType))
		{
			return false;
		}
	}

	return true;
}

bool UActionSkillComponent::UseSkill(FName SkillId, AActor* OptionalTarget, EActionSkillCancelFlag IncomingType)
{
	UActionSkillObject* SkillObject = GetSkillObject(SkillId);
	if (!CanUseSkill(SkillId, IncomingType) || SkillObject == nullptr)
	{
		UE_LOG(LogActionSkill, Verbose, TEXT("SkillComponent[%s]: UseSkill blocked. SkillId=%s"), *GetNameSafe(GetOwner()), *SkillId.ToString());
		return false;
	}

	if (CurrentSkillObject != nullptr)
	{
		StopSkill(EActionSkillStopReason::SkillCancel);
	}

	SkillObject->Activate(OptionalTarget);
	if (!SkillObject->IsActive())
	{
		return false;
	}

	CurrentSkillObject = SkillObject;
	ApplyActiveSkillTags(SkillObject->GetSkillData());

	if (AActionCharacterBase* OwnerCharacter = GetOwnerCharacter())
	{
		OwnerCharacter->RequestActionState(EActionCharacterState::Skill);
	}

	OnSkillStateChanged.Broadcast(SkillId, true);

	UE_LOG(LogActionSkill, Log, TEXT("SkillComponent[%s]: UseSkill started. SkillId=%s"), *GetNameSafe(GetOwner()), *SkillId.ToString());
	return true;
}

void UActionSkillComponent::StopSkill(EActionSkillStopReason Reason)
{
	if (CurrentSkillObject == nullptr)
	{
		return;
	}

	const FName StoppedSkillId = CurrentSkillObject->GetSkillId();
	CurrentSkillObject->Deactivate(Reason);
	CurrentSkillObject = nullptr;
	ClearActiveSkillTags();

	if (AActionCharacterBase* OwnerCharacter = GetOwnerCharacter())
	{
		if (!OwnerCharacter->IsDead() && OwnerCharacter->IsInActionState(EActionCharacterState::Skill))
		{
			OwnerCharacter->RequestActionState(EActionCharacterState::Idle);
		}
	}

	OnSkillStateChanged.Broadcast(StoppedSkillId, false);

	UE_LOG(
		LogActionSkill,
		Log,
		TEXT("SkillComponent[%s]: StopSkill. SkillId=%s Reason=%d"),
		*GetNameSafe(GetOwner()),
		*StoppedSkillId.ToString(),
		static_cast<uint8>(Reason));
}

void UActionSkillComponent::OnSkillNotify(FName EventName)
{
	UE_LOG(LogActionSkill, Verbose, TEXT("SkillComponent[%s]: Notify=%s"), *GetNameSafe(GetOwner()), *EventName.ToString());
}

bool UActionSkillComponent::CheckCanCancelCurrentSkill(EActionSkillCancelFlag IncomingType) const
{
	return CurrentSkillObject == nullptr || CurrentSkillObject->CanBeCancelledBy(IncomingType);
}

bool UActionSkillComponent::TryCancelCurrentSkill(EActionSkillCancelFlag IncomingType, EActionSkillStopReason Reason)
{
	if (CurrentSkillObject == nullptr)
	{
		return true;
	}

	if (!CheckCanCancelCurrentSkill(IncomingType))
	{
		UE_LOG(
			LogActionSkill,
			Verbose,
			TEXT("SkillComponent[%s]: cancel blocked. SkillId=%s IncomingType=%d"),
			*GetNameSafe(GetOwner()),
			*CurrentSkillObject->GetSkillId().ToString(),
			static_cast<uint8>(IncomingType));
		return false;
	}

	StopSkill(Reason);
	return true;
}

// DataTable 行是技能定义。这里把配置行转换成运行时对象，
// 确保激活状态、冷却、目标等运行时数据不会污染编辑器配置。
void UActionSkillComponent::LoadSkillObjectsFromTable()
{
	ClearSkillObjects();

	AActionCharacterBase* OwnerCharacter = GetOwnerCharacter();
	if (OwnerCharacter == nullptr)
	{
		return;
	}

	if (SkillDataTable == nullptr)
	{
		UE_LOG(LogActionSkill, Log, TEXT("SkillComponent[%s]: Loaded 0 skills from DataTable (table not configured)."), *GetNameSafe(GetOwner()));
		return;
	}

	if (SkillDataTable->GetRowStruct() != FActionSkillRow::StaticStruct())
	{
		UE_LOG(
			LogActionSkill,
			Warning,
			TEXT("SkillComponent[%s]: SkillDataTable row type mismatch. Expected FActionSkillRow."),
			*GetNameSafe(GetOwner()));
		return;
	}

	const TMap<FName, uint8*>& RowMap = SkillDataTable->GetRowMap();
	for (const TPair<FName, uint8*>& Pair : RowMap)
	{
		const FActionSkillRow* Row = reinterpret_cast<const FActionSkillRow*>(Pair.Value);
		if (Row == nullptr)
		{
			continue;
		}

		const FName SkillId = !Row->SkillId.IsNone() ? Row->SkillId : Pair.Key;
		if (SkillId.IsNone())
		{
			continue;
		}

		UActionSkillObject* SkillObject = NewObject<UActionSkillObject>(this);
		SkillObject->InitFromData(OwnerCharacter, SkillId, *Row);
		SkillObjectMap.Add(SkillId, SkillObject);
	}

	UE_LOG(
		LogActionSkill,
		Log,
		TEXT("SkillComponent[%s]: Loaded %d skills from DataTable."),
		*GetNameSafe(GetOwner()),
		SkillObjectMap.Num());
}

void UActionSkillComponent::ClearSkillObjects()
{
	StopSkill(EActionSkillStopReason::Forced);
	SkillObjectMap.Reset();
}

void UActionSkillComponent::ApplyActiveSkillTags(const FActionSkillRow& SkillData)
{
	ClearActiveSkillTags();

	AActionCharacterBase* OwnerCharacter = GetOwnerCharacter();
	if (OwnerCharacter == nullptr)
	{
		return;
	}

	ActiveSkillAppliedTags.AddTag(ActionGameplayTags::State_Action_Skill);
	if (SkillData.bBlockMove)
	{
		ActiveSkillAppliedTags.AddTag(ActionGameplayTags::Block_Move);
	}
	if (SkillData.bBlockDodge)
	{
		ActiveSkillAppliedTags.AddTag(ActionGameplayTags::Block_Dodge);
	}
	if (SkillData.bBlockAttack)
	{
		ActiveSkillAppliedTags.AddTag(ActionGameplayTags::Block_Attack);
	}

	for (TArray<FGameplayTag>::TConstIterator It = ActiveSkillAppliedTags.CreateConstIterator(); It; ++It)
	{
		OwnerCharacter->AddActionTagExternal(*It);
	}
}

void UActionSkillComponent::ClearActiveSkillTags()
{
	AActionCharacterBase* OwnerCharacter = GetOwnerCharacter();
	if (OwnerCharacter != nullptr)
	{
		for (TArray<FGameplayTag>::TConstIterator It = ActiveSkillAppliedTags.CreateConstIterator(); It; ++It)
		{
			OwnerCharacter->RemoveActionTagExternal(*It);
		}
	}

	ActiveSkillAppliedTags.Reset();
}

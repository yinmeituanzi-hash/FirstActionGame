#include "Combat/Skills/ActionSkillComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Char/ActionCharacterBase.h"
#include "Char/ActionPlayerCharacter.h"
#include "Combat/Attributes/ActionAttributeComponent.h"
#include "Combat/Components/ActionCombatComponent.h"
#include "Combat/Skills/ActionSkillNode.h"
#include "Combat/Skills/ActionSkillObject.h"
#include "Combat/VFX/ActionVFXComponent.h"
#include "Common/ActionGameplayTags.h"
#include "Engine/DataTable.h"
#include "Input/InputBufferComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionSkill, Log, All);

namespace
{
	FGameplayTag GetSkillWindowTagForCancelFlag(EActionSkillCancelFlag IncomingType)
	{
		switch (IncomingType)
		{
		case EActionSkillCancelFlag::NormalAttack:
			return ActionGameplayTags::Window_Skill_CanNormalAttackCancel;
		case EActionSkillCancelFlag::HeavyAttack:
			return ActionGameplayTags::Window_Skill_CanHeavyAttackCancel;
		case EActionSkillCancelFlag::Dodge:
			return ActionGameplayTags::Window_Skill_CanDodgeCancel;
		case EActionSkillCancelFlag::Skill:
			return ActionGameplayTags::Window_Skill_CanSkillCancel;
		case EActionSkillCancelFlag::Jump:
			return ActionGameplayTags::Window_Skill_CanJumpCancel;
		case EActionSkillCancelFlag::Ultimate:
			return ActionGameplayTags::Window_Skill_CanUltimateCancel;
		case EActionSkillCancelFlag::Move:
			return ActionGameplayTags::Window_Skill_CanMoveCancel;
		default:
			return FGameplayTag();
		}
	}

	void ForEachCancelFlagInMask(int32 CancelWindowMask, TFunctionRef<void(EActionSkillCancelFlag)> Visitor)
	{
		const EActionSkillCancelFlag Flags[] =
		{
			EActionSkillCancelFlag::NormalAttack,
			EActionSkillCancelFlag::HeavyAttack,
			EActionSkillCancelFlag::Dodge,
			EActionSkillCancelFlag::Skill,
			EActionSkillCancelFlag::Jump,
			EActionSkillCancelFlag::Ultimate,
			EActionSkillCancelFlag::Move
		};

		for (const EActionSkillCancelFlag Flag : Flags)
		{
			if ((CancelWindowMask & static_cast<int32>(Flag)) != 0)
			{
				Visitor(Flag);
			}
		}
	}
}

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

	TickComboTimeline();
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

		if (!CheckCanCancelCurrentSkill(IncomingType))
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

	if (!SkillObject->InitSkillNodes(this, SkillNodeDataTable, SkillEffectDataTable))
	{
		SkillObject->Deactivate(EActionSkillStopReason::Forced);
		UE_LOG(
			LogActionSkill,
			Warning,
			TEXT("SkillComponent[%s]: UseSkill failed because node map init failed. SkillId=%s"),
			*GetNameSafe(GetOwner()),
			*SkillId.ToString());
		return false;
	}

	CurrentSkillObject = SkillObject;
	ApplyActiveSkillTags(SkillObject->GetSkillData());

	if (AActionCharacterBase* OwnerCharacter = GetOwnerCharacter())
	{
		OwnerCharacter->RequestActionState(EActionCharacterState::Skill);
	}

	OnSkillStateChanged.Broadcast(SkillId, true);

	const FName BeginNodeId = SkillObject->GetSkillData().BeginNodeId;
	if (!CanAffordNodeCost(BeginNodeId))
	{
		UE_LOG(LogActionSkill, Verbose, TEXT("SkillComponent[%s]: UseSkill blocked — not enough SP for begin node %s."),
			*GetNameSafe(GetOwner()), *BeginNodeId.ToString());
		StopSkill(EActionSkillStopReason::Forced);
		return false;
	}

	PayNodeCost(BeginNodeId);
	StartSkillNode(BeginNodeId);

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
	StopActiveSkillMontage();
	DeactivateCurrentSkillNode();
	ClearActiveSkillWindows(false);
	CurrentSkillObject->Deactivate(Reason);
	CurrentSkillObject = nullptr;
	ClearActiveSkillTags();

	if (AActionCharacterBase* OwnerCharacter = GetOwnerCharacter())
	{
		if (UActionVFXComponent* VFXComponent = OwnerCharacter->GetActionVFXComponent())
		{
			VFXComponent->StopSkillLifetimeVFX(StoppedSkillId);
		}

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
	if (CurrentSkillNode != nullptr)
	{
		CurrentSkillNode->OnNotify(EventName);
	}
}

void UActionSkillComponent::OnComboWindowNotify(FName InputName, FName HoldType)
{
	if (CurrentSkillNode == nullptr)
	{
		return;
	}

	CurrentSkillNode->OnNotifyNextCombo(InputName, HoldType);
}

void UActionSkillComponent::OnQuitSkillNotify()
{
	if (CurrentSkillNode == nullptr)
	{
		return;
	}

	CurrentSkillNode->OnNotifyQuitSkill();
}

void UActionSkillComponent::OnTurnWindowNotify()
{
	if (CurrentSkillNode == nullptr)
	{
		return;
	}

	CurrentSkillNode->OnNotifyTurnWindow();
}

bool UActionSkillComponent::CheckCanCancelCurrentSkill(EActionSkillCancelFlag IncomingType) const
{
	if (CurrentSkillObject == nullptr)
	{
		return true;
	}

	return CurrentSkillObject->CanBeCancelledBy(IncomingType) && IsSkillCancelWindowOpen(IncomingType);
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

bool UActionSkillComponent::IsSkillCancelWindowOpen(EActionSkillCancelFlag IncomingType) const
{
	if (CurrentSkillObject == nullptr)
	{
		return true;
	}

	const FGameplayTag WindowTag = GetSkillWindowTagForCancelFlag(IncomingType);
	if (!WindowTag.IsValid())
	{
		return false;
	}

	const int32* Count = ActiveSkillWindowTagCounts.Find(WindowTag);
	return Count != nullptr && *Count > 0;
}

void UActionSkillComponent::OpenSkillCancelWindow(
	int32 CancelWindowMask,
	bool bReleaseMoveBlock,
	bool bReleaseDodgeBlock,
	bool bReleaseAttackBlock,
	bool bMarkRecoverWindow)
{
	if (CurrentSkillObject == nullptr)
	{
		return;
	}

	ForEachCancelFlagInMask(CancelWindowMask, [this](EActionSkillCancelFlag Flag)
	{
		AddWindowTagCount(GetSkillWindowTagForCancelFlag(Flag));
	});

	if (bMarkRecoverWindow)
	{
		AddWindowTagCount(ActionGameplayTags::Window_Skill_CanRecover);
	}

	if (bReleaseMoveBlock)
	{
		AddReleasedBlockCount(ActionGameplayTags::Block_Move);
	}
	if (bReleaseDodgeBlock)
	{
		AddReleasedBlockCount(ActionGameplayTags::Block_Dodge);
	}
	if (bReleaseAttackBlock)
	{
		AddReleasedBlockCount(ActionGameplayTags::Block_Attack);
	}
}

void UActionSkillComponent::CloseSkillCancelWindow(
	int32 CancelWindowMask,
	bool bReleaseMoveBlock,
	bool bReleaseDodgeBlock,
	bool bReleaseAttackBlock,
	bool bMarkRecoverWindow)
{
	if (CurrentSkillObject == nullptr)
	{
		return;
	}

	ForEachCancelFlagInMask(CancelWindowMask, [this](EActionSkillCancelFlag Flag)
	{
		RemoveWindowTagCount(GetSkillWindowTagForCancelFlag(Flag));
	});

	if (bMarkRecoverWindow)
	{
		RemoveWindowTagCount(ActionGameplayTags::Window_Skill_CanRecover);
	}

	if (bReleaseMoveBlock)
	{
		RemoveReleasedBlockCount(ActionGameplayTags::Block_Move);
	}
	if (bReleaseDodgeBlock)
	{
		RemoveReleasedBlockCount(ActionGameplayTags::Block_Dodge);
	}
	if (bReleaseAttackBlock)
	{
		RemoveReleasedBlockCount(ActionGameplayTags::Block_Attack);
	}
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

bool UActionSkillComponent::StartSkillNode(FName NodeId)
{
	if (NodeId.IsNone())
	{
		StopActiveSkillMontage();
		DeactivateCurrentSkillNode();
		ClearActiveSkillWindows(false);
		UE_LOG(LogActionSkill, Verbose, TEXT("SkillComponent[%s]: skill has no BeginNode."), *GetNameSafe(GetOwner()));
		return true;
	}

	const FActionSkillNodeRow* NodeRow = FindSkillNodeRow(NodeId);
	UActionSkillNode* TargetNode = CurrentSkillObject != nullptr ? CurrentSkillObject->GetSkillNode(NodeId) : nullptr;
	if (NodeRow == nullptr || CurrentSkillObject == nullptr || TargetNode == nullptr)
	{
		UE_LOG(
			LogActionSkill,
			Warning,
			TEXT("SkillComponent[%s]: failed to start skill node. NodeId=%s NodeRow=%s NodeObject=%s"),
			*GetNameSafe(GetOwner()),
			*NodeId.ToString(),
			NodeRow != nullptr ? TEXT("Valid") : TEXT("None"),
			TargetNode != nullptr ? TEXT("Valid") : TEXT("None"));
		StopSkill(EActionSkillStopReason::Forced);
		return false;
	}

	const bool bWillJumpSection = ActiveSkillMontage != nullptr
		&& NodeRow->Montage == ActiveSkillMontage
		&& !NodeRow->StartSection.IsNone();
	const UActionSkillNode* PreviousNode = CurrentSkillNode;

	if (!bWillJumpSection)
	{
		StopActiveSkillMontage();
	}

	ApplyTurnAtNodeStart(PreviousNode);
	DeactivateCurrentSkillNode();
	ClearActiveSkillWindows(false);

	CurrentSkillObject->ResetHitActorsThisNode();

	CurrentSkillNode = TargetNode;
	CurrentSkillNode->Activate();

	if (!PlayCurrentNodeMontage())
	{
		UE_LOG(
			LogActionSkill,
			Warning,
			TEXT("SkillComponent[%s]: node started without montage or montage failed. NodeId=%s"),
			*GetNameSafe(GetOwner()),
			*NodeId.ToString());
	}

	return true;
}

void UActionSkillComponent::ApplyTurnAtNodeStart(const UActionSkillNode* PreviousNode)
{
	if (PreviousNode == nullptr || !PreviousNode->CanTurnNextNode())
	{
		return;
	}

	AActionPlayerCharacter* PlayerOwner = Cast<AActionPlayerCharacter>(GetOwnerCharacter());
	if (PlayerOwner == nullptr)
	{
		return;
	}

	UActionCombatComponent* CombatComp = PlayerOwner->GetActionCombatComponent();
	if (CombatComp == nullptr)
	{
		return;
	}

	if (PlayerOwner->HasLockOnTarget())
	{
		const FVector ToTarget = (PlayerOwner->GetLockOnTargetLocation() - PlayerOwner->GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			CombatComp->ApplyAttackTurnToWorldYaw(PlayerOwner, ToTarget.Rotation().Yaw);
		}
		return;
	}

	CombatComp->ApplyAttackTurnAtComboStart(PlayerOwner, PlayerOwner->GetLastMoveInput());
}

void UActionSkillComponent::TickComboTimeline()
{
	if (CurrentSkillObject == nullptr || CurrentSkillNode == nullptr)
	{
		return;
	}

	const FName NextNodeId = CurrentSkillNode->CheckComboTransition();
	if (!NextNodeId.IsNone())
	{
		if (!CanAffordNodeCost(NextNodeId))
		{
			UE_LOG(LogActionSkill, Verbose, TEXT("SkillComponent[%s]: combo transition blocked — not enough SP for node %s."),
				*GetNameSafe(GetOwner()), *NextNodeId.ToString());
		}
		else
		{
			ConsumeComboInput();
			PayNodeCost(NextNodeId);
			StartSkillNode(NextNodeId);
			return;
		}
	}

	CurrentSkillNode->TickByTimeLine();

	if (CurrentSkillNode != nullptr && CurrentSkillNode->ShouldQuitSkill())
	{
		StopSkill(EActionSkillStopReason::Normal);
	}
}

void UActionSkillComponent::ConsumeComboInput()
{
	if (CurrentSkillNode == nullptr)
	{
		return;
	}

	const FName InputName = CurrentSkillNode->GetLastMatchedComboInputName();
	if (InputName.IsNone())
	{
		return;
	}

	AActionCharacterBase* OwnerCharacter = GetOwnerCharacter();
	UInputBufferComponent* InputBuffer = OwnerCharacter != nullptr
		? OwnerCharacter->FindComponentByClass<UInputBufferComponent>()
		: nullptr;
	if (InputBuffer != nullptr)
	{
		InputBuffer->ConsumeInput(InputName);
	}
}

bool UActionSkillComponent::CanAffordNodeCost(FName NodeId) const
{
	if (NodeId.IsNone())
	{
		return true;
	}

	const FActionSkillNodeRow* NodeRow = FindSkillNodeRow(NodeId);
	if (NodeRow == nullptr || NodeRow->CostSP <= 0)
	{
		return true;
	}

	const AActionCharacterBase* OwnerCharacter = GetOwnerCharacter();
	const UActionAttributeComponent* AttrComp = OwnerCharacter != nullptr
		? OwnerCharacter->FindComponentByClass<UActionAttributeComponent>()
		: nullptr;
	if (AttrComp == nullptr)
	{
		return true;
	}

	return AttrComp->GetAttribute(EActionAttributeType::SP) >= static_cast<float>(NodeRow->CostSP);
}

void UActionSkillComponent::PayNodeCost(FName NodeId)
{
	if (NodeId.IsNone())
	{
		return;
	}

	const FActionSkillNodeRow* NodeRow = FindSkillNodeRow(NodeId);
	if (NodeRow == nullptr || NodeRow->CostSP <= 0)
	{
		return;
	}

	AActionCharacterBase* OwnerCharacter = GetOwnerCharacter();
	UActionAttributeComponent* AttrComp = OwnerCharacter != nullptr
		? OwnerCharacter->FindComponentByClass<UActionAttributeComponent>()
		: nullptr;
	if (AttrComp == nullptr)
	{
		return;
	}

	AttrComp->ModifyAttribute(EActionAttributeType::SP, -static_cast<float>(NodeRow->CostSP));
	UE_LOG(LogActionSkill, Verbose, TEXT("SkillComponent[%s]: paid SP cost %d for node %s. Remaining SP=%.0f"),
		*GetNameSafe(GetOwner()),
		NodeRow->CostSP,
		*NodeId.ToString(),
		AttrComp->GetAttribute(EActionAttributeType::SP));
}

const FActionSkillNodeRow* UActionSkillComponent::FindSkillNodeRow(FName NodeId) const
{
	if (SkillNodeDataTable == nullptr || SkillNodeDataTable->GetRowStruct() != FActionSkillNodeRow::StaticStruct())
	{
		return nullptr;
	}

	return SkillNodeDataTable->FindRow<FActionSkillNodeRow>(NodeId, TEXT("ActionSkillComponent.FindSkillNodeRow"));
}

UAnimInstance* UActionSkillComponent::GetOwnerAnimInstance() const
{
	const AActionCharacterBase* OwnerCharacter = GetOwnerCharacter();
	return OwnerCharacter != nullptr && OwnerCharacter->GetMesh() != nullptr ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr;
}

bool UActionSkillComponent::PlayCurrentNodeMontage()
{
	if (CurrentSkillNode == nullptr)
	{
		return false;
	}

	const FActionSkillNodeRow& NodeData = CurrentSkillNode->GetNodeData();
	if (NodeData.Montage == nullptr)
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetOwnerAnimInstance();
	if (AnimInstance == nullptr)
	{
		return false;
	}

	const bool bSameActiveMontage = ActiveSkillMontage == NodeData.Montage
		&& AnimInstance->Montage_IsActive(ActiveSkillMontage);

	if (bSameActiveMontage && !NodeData.StartSection.IsNone())
	{
		AnimInstance->Montage_JumpToSection(NodeData.StartSection, ActiveSkillMontage);

		UE_LOG(
			LogActionSkill,
			Log,
			TEXT("SkillComponent[%s]: JumpToSection. Skill=%s Node=%s Section=%s Montage=%s"),
			*GetNameSafe(GetOwner()),
			*GetCurrentSkillId().ToString(),
			*CurrentSkillNode->GetNodeId().ToString(),
			*NodeData.StartSection.ToString(),
			*GetNameSafe(ActiveSkillMontage));

		return true;
	}

	StopActiveSkillMontage();

	const float Duration = AnimInstance->Montage_Play(NodeData.Montage, NodeData.PlayRate);
	if (Duration <= 0.0f)
	{
		return false;
	}

	ActiveSkillMontage = NodeData.Montage;
	if (!NodeData.StartSection.IsNone())
	{
		AnimInstance->Montage_JumpToSection(NodeData.StartSection, ActiveSkillMontage);
	}

	AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UActionSkillComponent::HandleSkillMontageNotifyBegin);
	AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &UActionSkillComponent::HandleSkillMontageNotifyBegin);

	UE_LOG(
		LogActionSkill,
		Log,
		TEXT("SkillComponent[%s]: playing skill montage. Skill=%s Node=%s Montage=%s Duration=%.2f"),
		*GetNameSafe(GetOwner()),
		*GetCurrentSkillId().ToString(),
		*CurrentSkillNode->GetNodeId().ToString(),
		*GetNameSafe(ActiveSkillMontage),
		Duration);

	return true;
}

void UActionSkillComponent::StopActiveSkillMontage(float BlendOutTime)
{
	UAnimInstance* AnimInstance = GetOwnerAnimInstance();
	if (AnimInstance == nullptr || ActiveSkillMontage == nullptr)
	{
		ActiveSkillMontage = nullptr;
		return;
	}

	UAnimMontage* MontageToStop = ActiveSkillMontage;
	ClearSkillMontageDelegates(AnimInstance);
	ActiveSkillMontage = nullptr;

	if (AnimInstance->Montage_IsPlaying(MontageToStop))
	{
		const float UseBlendOut = BlendOutTime >= 0.0f
			? BlendOutTime
			: (CurrentSkillNode != nullptr ? CurrentSkillNode->GetNodeData().MontageBlendOutTime : 0.1f);
		AnimInstance->Montage_Stop(UseBlendOut, MontageToStop);
	}
}

void UActionSkillComponent::ClearSkillMontageDelegates(UAnimInstance* AnimInstance)
{
	if (AnimInstance == nullptr)
	{
		return;
	}

	AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UActionSkillComponent::HandleSkillMontageNotifyBegin);
}

void UActionSkillComponent::DeactivateCurrentSkillNode()
{
	if (CurrentSkillNode != nullptr)
	{
		CurrentSkillNode->Deactivate();
		CurrentSkillNode = nullptr;
	}
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

void UActionSkillComponent::AddWindowTagCount(FGameplayTag Tag)
{
	if (!Tag.IsValid())
	{
		return;
	}

	int32& Count = ActiveSkillWindowTagCounts.FindOrAdd(Tag);
	++Count;
	if (Count == 1)
	{
		if (AActionCharacterBase* OwnerCharacter = GetOwnerCharacter())
		{
			OwnerCharacter->AddActionTagExternal(Tag);
		}
	}
}

void UActionSkillComponent::RemoveWindowTagCount(FGameplayTag Tag)
{
	if (!Tag.IsValid())
	{
		return;
	}

	int32* Count = ActiveSkillWindowTagCounts.Find(Tag);
	if (Count == nullptr)
	{
		return;
	}

	--(*Count);
	if (*Count <= 0)
	{
		ActiveSkillWindowTagCounts.Remove(Tag);
		if (AActionCharacterBase* OwnerCharacter = GetOwnerCharacter())
		{
			OwnerCharacter->RemoveActionTagExternal(Tag);
		}
	}
}

void UActionSkillComponent::AddReleasedBlockCount(FGameplayTag Tag)
{
	if (!Tag.IsValid())
	{
		return;
	}

	int32& Count = ActiveSkillReleasedBlockCounts.FindOrAdd(Tag);
	++Count;
	if (Count == 1)
	{
		if (AActionCharacterBase* OwnerCharacter = GetOwnerCharacter())
		{
			OwnerCharacter->RemoveActionTagExternal(Tag);
		}
	}
}

void UActionSkillComponent::RemoveReleasedBlockCount(FGameplayTag Tag)
{
	if (!Tag.IsValid())
	{
		return;
	}

	int32* Count = ActiveSkillReleasedBlockCounts.Find(Tag);
	if (Count == nullptr)
	{
		return;
	}

	--(*Count);
	if (*Count <= 0)
	{
		ActiveSkillReleasedBlockCounts.Remove(Tag);
		if (ShouldRestoreReleasedBlock(Tag))
		{
			if (AActionCharacterBase* OwnerCharacter = GetOwnerCharacter())
			{
				OwnerCharacter->AddActionTagExternal(Tag);
			}
		}
	}
}

void UActionSkillComponent::ClearActiveSkillWindows(bool bRestoreReleasedBlocks)
{
	AActionCharacterBase* OwnerCharacter = GetOwnerCharacter();
	if (OwnerCharacter != nullptr)
	{
		for (const TPair<FGameplayTag, int32>& Pair : ActiveSkillWindowTagCounts)
		{
			OwnerCharacter->RemoveActionTagExternal(Pair.Key);
		}

		if (bRestoreReleasedBlocks)
		{
			for (const TPair<FGameplayTag, int32>& Pair : ActiveSkillReleasedBlockCounts)
			{
				if (ShouldRestoreReleasedBlock(Pair.Key))
				{
					OwnerCharacter->AddActionTagExternal(Pair.Key);
				}
			}
		}
	}

	ActiveSkillWindowTagCounts.Reset();
	ActiveSkillReleasedBlockCounts.Reset();
}

bool UActionSkillComponent::ShouldRestoreReleasedBlock(FGameplayTag Tag) const
{
	return CurrentSkillObject != nullptr && ActiveSkillAppliedTags.HasTagExact(Tag);
}

void UActionSkillComponent::HandleSkillMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& /*BranchingPointPayload*/)
{
	if (ActiveSkillMontage == nullptr || CurrentSkillNode == nullptr)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetOwnerAnimInstance();
	if (AnimInstance == nullptr || !AnimInstance->Montage_IsPlaying(ActiveSkillMontage))
	{
		return;
	}

	OnSkillNotify(NotifyName);
}

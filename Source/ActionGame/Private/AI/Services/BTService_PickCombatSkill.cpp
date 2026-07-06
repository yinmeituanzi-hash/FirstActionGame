#include "AI/Services/BTService_PickCombatSkill.h"

#include "AI/ActionAIBlackboardKeys.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Char/ActionCharacterBase.h"
#include "Combat/Skills/ActionSkillComponent.h"
#include "Combat/Skills/ActionSkillObject.h"

UBTService_PickCombatSkill::UBTService_PickCombatSkill()
{
	NodeName = TEXT("Pick Combat Skill");
	Interval = 0.25f;
	RandomDeviation = 0.05f;

	TargetActorKey.SelectedKeyName = ActionAIBlackboardKeys::TargetActor;
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_PickCombatSkill, TargetActorKey), AActor::StaticClass());

	SelectedSkillIdKey.SelectedKeyName = ActionAIBlackboardKeys::SelectedSkillId;
	SelectedSkillIdKey.AddNameFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_PickCombatSkill, SelectedSkillIdKey));

	IsSelectedSkillInRangeKey.SelectedKeyName = ActionAIBlackboardKeys::IsSelectedSkillInRange;
	IsSelectedSkillInRangeKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_PickCombatSkill, IsSelectedSkillInRangeKey));

	ShouldApproachSelectedSkillKey.SelectedKeyName = ActionAIBlackboardKeys::ShouldApproachSelectedSkill;
	ShouldApproachSelectedSkillKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_PickCombatSkill, ShouldApproachSelectedSkillKey));

	SelectedSkillMinRangeKey.SelectedKeyName = ActionAIBlackboardKeys::SelectedSkillMinRange;
	SelectedSkillMinRangeKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_PickCombatSkill, SelectedSkillMinRangeKey));

	SelectedSkillMaxRangeKey.SelectedKeyName = ActionAIBlackboardKeys::SelectedSkillMaxRange;
	SelectedSkillMaxRangeKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_PickCombatSkill, SelectedSkillMaxRangeKey));

	SelectedSkillPreferredRangeKey.SelectedKeyName = ActionAIBlackboardKeys::SelectedSkillPreferredRange;
	SelectedSkillPreferredRangeKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_PickCombatSkill, SelectedSkillPreferredRangeKey));
}

void UBTService_PickCombatSkill::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*BBAsset);
		SelectedSkillIdKey.ResolveSelectedKey(*BBAsset);
		IsSelectedSkillInRangeKey.ResolveSelectedKey(*BBAsset);
		ShouldApproachSelectedSkillKey.ResolveSelectedKey(*BBAsset);
		SelectedSkillMinRangeKey.ResolveSelectedKey(*BBAsset);
		SelectedSkillMaxRangeKey.ResolveSelectedKey(*BBAsset);
		SelectedSkillPreferredRangeKey.ResolveSelectedKey(*BBAsset);
	}
}

void UBTService_PickCombatSkill::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* AIOwner = OwnerComp.GetAIOwner();
	AActionCharacterBase* OwnerCharacter = AIOwner != nullptr ? Cast<AActionCharacterBase>(AIOwner->GetCharacter()) : nullptr;
	UActionSkillComponent* SkillComponent = OwnerCharacter != nullptr ? OwnerCharacter->GetActionSkillComponent() : nullptr;
	AActor* TargetActor = Blackboard != nullptr ? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;
	if (Blackboard == nullptr || OwnerCharacter == nullptr || OwnerCharacter->IsDead() || SkillComponent == nullptr || TargetActor == nullptr)
	{
		if (Blackboard != nullptr)
		{
			ClearSelection(*Blackboard);
		}
		return;
	}

	// 技能已经真正启动后，选招结果需要冻结到技能结束。
	// 否则当前技能进入 CD / 当前技能占用状态会让 CanUseSkill 变 false，
	// 进而把 IsSelectedSkillInRange 写回 false，导致 BT Decorator 在挥刀中途 Abort 到走位分支。
	if (SkillComponent->IsUsingSkill())
	{
		return;
	}

	const float DistanceToTarget = FVector::Dist2D(OwnerCharacter->GetActorLocation(), TargetActor->GetActorLocation());
	TArray<FRuntimeCandidate> CastableCandidates;
	TArray<FRuntimeCandidate> ReadyReferenceCandidates;
	TArray<FRuntimeCandidate> CooldownReferenceCandidates;

	for (const FActionAICombatSkillCandidate& Config : CandidateSkills)
	{
		if (Config.SkillId.IsNone() || Config.Weight <= 0 || SkillComponent->GetSkillObject(Config.SkillId) == nullptr)
		{
			continue;
		}

		FRuntimeCandidate Candidate;
		Candidate.SkillId = Config.SkillId;
		Candidate.Weight = Config.Weight;
		Candidate.CooldownRemaining = SkillComponent->GetSkillCooldownRemaining(Config.SkillId);

		if (!SkillComponent->GetSkillReleaseRange(Candidate.SkillId, Candidate.MinRange, Candidate.MaxRange, Candidate.PreferredRange))
		{
			Candidate.MinRange = FMath::Max(0.0f, DefaultMinReleaseRange);
			Candidate.MaxRange = FMath::Max(Candidate.MinRange, DefaultMaxReleaseRange);
			Candidate.PreferredRange = DefaultPreferredReleaseRange > 0.0f
				? FMath::Clamp(DefaultPreferredReleaseRange, Candidate.MinRange, Candidate.MaxRange)
				: Candidate.MaxRange;
		}

		Candidate.bInRange = DistanceToTarget >= Candidate.MinRange && DistanceToTarget <= Candidate.MaxRange;
		Candidate.bCanUseNow = SkillComponent->CanUseSkill(Candidate.SkillId, ESkillCancelFlag::Skill);

		if (Candidate.bCanUseNow && Candidate.bInRange)
		{
			CastableCandidates.Add(Candidate);
		}
		else if (Candidate.bCanUseNow)
		{
			ReadyReferenceCandidates.Add(Candidate);
		}
		else
		{
			CooldownReferenceCandidates.Add(Candidate);
		}
	}

	if (const FRuntimeCandidate* Picked = PickWeighted(CastableCandidates))
	{
		WriteSelection(*Blackboard, *Picked, DistanceToTarget);
		return;
	}

	if (const FRuntimeCandidate* Picked = PickWeighted(ReadyReferenceCandidates))
	{
		WriteSelection(*Blackboard, *Picked, DistanceToTarget);
		return;
	}

	if (CooldownReferenceCandidates.Num() > 0)
	{
		CooldownReferenceCandidates.Sort([](const FRuntimeCandidate& A, const FRuntimeCandidate& B)
		{
			return A.CooldownRemaining < B.CooldownRemaining;
		});
		WriteSelection(*Blackboard, CooldownReferenceCandidates[0], DistanceToTarget);
		return;
	}

	ClearSelection(*Blackboard);
}

FString UBTService_PickCombatSkill::GetStaticDescription() const
{
	return FString::Printf(TEXT("PickCombatSkill: %d candidate(s)"), CandidateSkills.Num());
}

void UBTService_PickCombatSkill::ClearSelection(UBlackboardComponent& Blackboard) const
{
	Blackboard.ClearValue(SelectedSkillIdKey.SelectedKeyName);
	Blackboard.SetValueAsBool(IsSelectedSkillInRangeKey.SelectedKeyName, false);
	Blackboard.SetValueAsBool(ShouldApproachSelectedSkillKey.SelectedKeyName, false);
	Blackboard.SetValueAsFloat(SelectedSkillMinRangeKey.SelectedKeyName, 0.0f);
	Blackboard.SetValueAsFloat(SelectedSkillMaxRangeKey.SelectedKeyName, 0.0f);
	Blackboard.SetValueAsFloat(SelectedSkillPreferredRangeKey.SelectedKeyName, 0.0f);
}

void UBTService_PickCombatSkill::WriteSelection(UBlackboardComponent& Blackboard, const FRuntimeCandidate& Candidate, float DistanceToTarget) const
{
	Blackboard.SetValueAsName(SelectedSkillIdKey.SelectedKeyName, Candidate.SkillId);
	Blackboard.SetValueAsBool(IsSelectedSkillInRangeKey.SelectedKeyName, Candidate.bCanUseNow && Candidate.bInRange);
	Blackboard.SetValueAsBool(ShouldApproachSelectedSkillKey.SelectedKeyName, Candidate.bCanUseNow && DistanceToTarget > Candidate.MaxRange);
	Blackboard.SetValueAsFloat(SelectedSkillMinRangeKey.SelectedKeyName, Candidate.MinRange);
	Blackboard.SetValueAsFloat(SelectedSkillMaxRangeKey.SelectedKeyName, Candidate.MaxRange);
	Blackboard.SetValueAsFloat(SelectedSkillPreferredRangeKey.SelectedKeyName, Candidate.PreferredRange);
}

const UBTService_PickCombatSkill::FRuntimeCandidate* UBTService_PickCombatSkill::PickWeighted(const TArray<FRuntimeCandidate>& Candidates)
{
	int32 TotalWeight = 0;
	for (const FRuntimeCandidate& Candidate : Candidates)
	{
		TotalWeight += FMath::Max(0, Candidate.Weight);
	}

	if (TotalWeight <= 0)
	{
		return nullptr;
	}

	int32 Roll = FMath::RandRange(1, TotalWeight);
	for (const FRuntimeCandidate& Candidate : Candidates)
	{
		Roll -= FMath::Max(0, Candidate.Weight);
		if (Roll <= 0)
		{
			return &Candidate;
		}
	}

	return Candidates.Num() > 0 ? &Candidates.Last() : nullptr;
}

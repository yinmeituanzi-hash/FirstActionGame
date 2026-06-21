#include "AI/Tasks/BTTask_UseSkill.h"

#include "AI/ActionAIBlackboardKeys.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Char/ActionCharacterBase.h"
#include "Combat/Skills/ActionSkillComponent.h"

UBTTask_UseSkill::UBTTask_UseSkill()
{
	NodeName = TEXT("Use Skill");
	bNotifyTick = true;

	TargetActorKey.SelectedKeyName = ActionAIBlackboardKeys::TargetActor;
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_UseSkill, TargetActorKey), AActor::StaticClass());
}

void UBTTask_UseSkill::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_UseSkill::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTUseSkillMemory* Memory = reinterpret_cast<FBTUseSkillMemory*>(NodeMemory);
	Memory->ElapsedTime = 0.0f;
	Memory->bSkillStarted = false;

	if (SkillId.IsNone())
	{
		return EBTNodeResult::Failed;
	}

	AAIController* AIOwner = OwnerComp.GetAIOwner();
	AActionCharacterBase* OwnerCharacter = AIOwner != nullptr ? Cast<AActionCharacterBase>(AIOwner->GetCharacter()) : nullptr;
	UActionSkillComponent* SkillComponent = OwnerCharacter != nullptr ? OwnerCharacter->GetActionSkillComponent() : nullptr;
	if (OwnerCharacter == nullptr || OwnerCharacter->IsDead() || SkillComponent == nullptr)
	{
		return EBTNodeResult::Failed;
	}

	AActor* TargetActor = GetBlackboardTarget(OwnerComp);
	if (!SkillComponent->IsTargetInSkillReleaseRange(SkillId, TargetActor))
	{
		return EBTNodeResult::Failed;
	}

	// 010 的 UseSkill Task 也是先做可用性检查，失败就立刻 Failed。
	// 冷却中还没真正启动技能时不能卡在本节点，否则目标跑远后会原地放空技能。
	if (!SkillComponent->CanUseSkill(SkillId, EActionSkillCancelFlag::Skill))
	{
		return EBTNodeResult::Failed;
	}
	return TryStartSkill(OwnerComp, *Memory) ? EBTNodeResult::InProgress : EBTNodeResult::Failed;
}

void UBTTask_UseSkill::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTUseSkillMemory* Memory = reinterpret_cast<FBTUseSkillMemory*>(NodeMemory);
	Memory->ElapsedTime += DeltaSeconds;

	AAIController* AIOwner = OwnerComp.GetAIOwner();
	AActionCharacterBase* OwnerCharacter = AIOwner != nullptr ? Cast<AActionCharacterBase>(AIOwner->GetCharacter()) : nullptr;
	UActionSkillComponent* SkillComponent = OwnerCharacter != nullptr ? OwnerCharacter->GetActionSkillComponent() : nullptr;
	if (OwnerCharacter == nullptr || OwnerCharacter->IsDead() || SkillComponent == nullptr)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (!Memory->bSkillStarted)
	{
		if (!TryStartSkill(OwnerComp, *Memory))
		{
			FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		}
		return;
	}

	if (!SkillComponent->IsUsingSkill() || SkillComponent->GetCurrentSkillId() != SkillId)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	if (Memory->ElapsedTime >= MaxExecutionTime)
	{
		// 技能时间线异常时的兜底。正常技能应通过 Montage / QuitSkill 自然结束。
		SkillComponent->StopSkill(EActionSkillStopReason::Forced);
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_UseSkill::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const FBTUseSkillMemory* Memory = reinterpret_cast<const FBTUseSkillMemory*>(NodeMemory);
	if (bStopSkillOnAbort && Memory != nullptr && Memory->bSkillStarted)
	{
		if (AAIController* AIOwner = OwnerComp.GetAIOwner())
		{
			if (AActionCharacterBase* OwnerCharacter = Cast<AActionCharacterBase>(AIOwner->GetCharacter()))
			{
				if (UActionSkillComponent* SkillComponent = OwnerCharacter->GetActionSkillComponent())
				{
					if (SkillComponent->GetCurrentSkillId() == SkillId)
					{
						SkillComponent->StopSkill(EActionSkillStopReason::Forced);
					}
				}
			}
		}
	}

	return EBTNodeResult::Aborted;
}

AActor* UBTTask_UseSkill::GetBlackboardTarget(const UBehaviorTreeComponent& OwnerComp) const
{
	const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (BB == nullptr || TargetActorKey.SelectedKeyName == NAME_None)
	{
		return nullptr;
	}

	return Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
}

bool UBTTask_UseSkill::TryStartSkill(UBehaviorTreeComponent& OwnerComp, FBTUseSkillMemory& Memory) const
{
	AAIController* AIOwner = OwnerComp.GetAIOwner();
	AActionCharacterBase* OwnerCharacter = AIOwner != nullptr ? Cast<AActionCharacterBase>(AIOwner->GetCharacter()) : nullptr;
	UActionSkillComponent* SkillComponent = OwnerCharacter != nullptr ? OwnerCharacter->GetActionSkillComponent() : nullptr;
	if (OwnerCharacter == nullptr || OwnerCharacter->IsDead() || SkillComponent == nullptr)
	{
		return false;
	}

	if (!SkillComponent->CanUseSkill(SkillId, EActionSkillCancelFlag::Skill))
	{
		return false;
	}

	AActor* TargetActor = GetBlackboardTarget(OwnerComp);
	if (!SkillComponent->IsTargetInSkillReleaseRange(SkillId, TargetActor))
	{
		return false;
	}

	if (!SkillComponent->UseSkill(SkillId, TargetActor, EActionSkillCancelFlag::Skill))
	{
		return false;
	}

	Memory.bSkillStarted = true;
	return true;
}

FString UBTTask_UseSkill::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("UseSkill: %s\nTarget: %s\nTimeout: %.1fs"),
		*SkillId.ToString(),
		*TargetActorKey.SelectedKeyName.ToString(),
		MaxExecutionTime);
}

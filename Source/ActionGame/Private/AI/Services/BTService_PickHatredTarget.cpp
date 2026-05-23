#include "AI/Services/BTService_PickHatredTarget.h"

#include "AI/ActionAIBlackboardKeys.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Char/ActionMonsterCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogPickHatredTarget, Log, All);

int32 UBTService_PickHatredTarget::GDebugDrawHatred = 0;

static FAutoConsoleVariableRef CVarAIHatredDebug(
	TEXT("AI.HatredDebug"),
	UBTService_PickHatredTarget::GDebugDrawHatred,
	TEXT("0=off, 1=draw Top3 hatred entries above each monster head."),
	ECVF_Cheat);

UBTService_PickHatredTarget::UBTService_PickHatredTarget()
{
	NodeName = TEXT("Pick Hatred Target");
	// 仇恨更新频率比视觉低就行；视觉是"现在能不能看到"，仇恨是"过去几秒被谁打"。
	Interval = 0.5f;
	RandomDeviation = 0.05f;

	TargetActorKey.SelectedKeyName = ActionAIBlackboardKeys::TargetActor;
}

void UBTService_PickHatredTarget::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*BBAsset);
	}
}

void UBTService_PickHatredTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* AIOwner = OwnerComp.GetAIOwner();
	AActionMonsterCharacter* Monster = AIOwner != nullptr ? Cast<AActionMonsterCharacter>(AIOwner->GetCharacter()) : nullptr;
	if (Blackboard == nullptr || Monster == nullptr || Monster->IsDead())
	{
		return;
	}

	AActor* HatredTarget = Monster->GetHighestHatredTarget();
	if (HatredTarget != nullptr)
	{
		Blackboard->SetValueAsObject(TargetActorKey.SelectedKeyName, HatredTarget);
	}
	else if (bClearTargetWhenHatredEmpty)
	{
		Blackboard->ClearValue(TargetActorKey.SelectedKeyName);
	}
	// else: 保留 VisionService 写的 TargetActor，让"看到了但还没被打"的怪也能锁定玩家。

	if (GDebugDrawHatred > 0)
	{
		DrawHatredDebug(Monster);
	}
}

FString UBTService_PickHatredTarget::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("PickHatredTarget: %s when hatred empty"),
		bClearTargetWhenHatredEmpty ? TEXT("clear TargetActor") : TEXT("keep TargetActor"));
}

void UBTService_PickHatredTarget::DrawHatredDebug(AActionMonsterCharacter* Monster) const
{
	if (Monster == nullptr)
	{
		return;
	}
	UWorld* World = Monster->GetWorld();
	if (World == nullptr)
	{
		return;
	}

	TArray<AActionMonsterCharacter::FHatredEntry> Top;
	Monster->GetHatredTopEntries(3, Top);

	// 在怪头顶 200cm 处叠加几行文本。Interval 是 0.5s，所以 LifeTime 设 0.6s 保证视觉不闪。
	const FVector BaseLoc = Monster->GetActorLocation() + FVector(0, 0, 200);
	const FString Header = FString::Printf(TEXT("[%s] Hatred (top %d / %d)"),
		*Monster->GetName(), Top.Num(), Monster->GetHatredEntryCount());
	DrawDebugString(World, BaseLoc, Header, nullptr, FColor::Yellow, 0.6f, false, 1.2f);

	for (int32 i = 0; i < Top.Num(); ++i)
	{
		const auto& Entry = Top[i];
		AActor* Target = Entry.Target.Get();
		const FString Line = FString::Printf(TEXT("  %d. %s = %.0f"),
			i + 1,
			Target != nullptr ? *Target->GetName() : TEXT("<invalid>"),
			Entry.Value);
		DrawDebugString(
			World,
			BaseLoc - FVector(0, 0, (i + 1) * 18),
			Line,
			nullptr,
			i == 0 ? FColor::Red : FColor::White,
			0.6f,
			false,
			1.0f);
	}
}

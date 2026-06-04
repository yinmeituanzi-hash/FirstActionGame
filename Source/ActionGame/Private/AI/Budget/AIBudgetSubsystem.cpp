#include "AI/Budget/AIBudgetSubsystem.h"

#include "AI/Budget/AIBudgetComponent.h"
#include "AI/Significance/AISignificanceComponent.h"
#include "Char/ActionMonsterCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

static TAutoConsoleVariable<int32> CVarAIBudgetEnable(
	TEXT("AI.BudgetEnable"),
	1,
	TEXT("Enable global AI tick budget allocation. 0=off, 1=on."),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarAIBudgetDebug(
	TEXT("AI.BudgetDebug"),
	0,
	TEXT("Draw AI budget allocation info. 0=off, 1=on."),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarAIBudgetMaxActive(
	TEXT("AI.BudgetMaxActive"),
	-1,
	TEXT("Override maximum active AI count. -1 uses subsystem config."),
	ECVF_Cheat);

namespace
{
FString BudgetReasonToString(EAIBudgetAllocationReason Reason)
{
	switch (Reason)
	{
	case EAIBudgetAllocationReason::UnderBudget:
		return TEXT("UnderBudget");
	case EAIBudgetAllocationReason::InQuota:
		return TEXT("InQuota");
	case EAIBudgetAllocationReason::Protected:
		return TEXT("Protected");
	case EAIBudgetAllocationReason::Overflow:
		return TEXT("Overflow");
	case EAIBudgetAllocationReason::SystemDisabled:
		return TEXT("Disabled");
	case EAIBudgetAllocationReason::Throttled:
		return TEXT("Throttled");
	default:
		return TEXT("Unknown");
	}
}

struct FAIBudgetCandidate
{
	UAIBudgetComponent* Budget = nullptr;
	float Score = 0.0f;
	float Distance = TNumericLimits<float>::Max();
	bool bProtected = false;
};
}

UAIBudgetSubsystem* UAIBudgetSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject != nullptr ? WorldContextObject->GetWorld() : nullptr;
	return World != nullptr ? World->GetSubsystem<UAIBudgetSubsystem>() : nullptr;
}

void UAIBudgetSubsystem::Tick(float DeltaTime)
{
	TimeSinceLastUpdate += DeltaTime;
	if (TimeSinceLastUpdate < ReevaluateInterval)
	{
		return;
	}

	TimeSinceLastUpdate = 0.0f;
	UpdateBudget();
}

TStatId UAIBudgetSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAIBudgetSubsystem, STATGROUP_Tickables);
}

bool UAIBudgetSubsystem::IsTickable() const
{
	return !IsTemplate()
		&& GetWorld() != nullptr
		&& !GetWorld()->IsNetMode(NM_Client);
}

void UAIBudgetSubsystem::RegisterComponent(UAIBudgetComponent* Component)
{
	if (Component != nullptr)
	{
		RegisteredComponents.AddUnique(Component);
	}
}

void UAIBudgetSubsystem::UnregisterComponent(UAIBudgetComponent* Component)
{
	RegisteredComponents.Remove(Component);
}

void UAIBudgetSubsystem::UpdateBudget()
{
	TArray<FAIBudgetCandidate> Candidates;
	Candidates.Reserve(RegisteredComponents.Num());

	for (int32 Index = RegisteredComponents.Num() - 1; Index >= 0; --Index)
	{
		UAIBudgetComponent* Budget = RegisteredComponents[Index].Get();
		AActionMonsterCharacter* Monster = Budget != nullptr ? Budget->GetOwnerMonster() : nullptr;
		if (Budget == nullptr || Monster == nullptr)
		{
			RegisteredComponents.RemoveAtSwap(Index);
			continue;
		}

		if (Monster->IsDead())
		{
			// 死亡表现由 Character 自己管理，Budget 不应继续持有 Tick 限制。
			// Character::Die 已立即释放一次；这里兼顾外部状态修改和未来对象池。
			Budget->ApplyBudgetEnabled(true, EAIBudgetAllocationReason::Protected, true);
			continue;
		}

		const UAISignificanceComponent* Significance = Monster->GetSignificanceComponent();
		if (Significance != nullptr && !Significance->IsBehaviorTreeAllowedBySignificance())
		{
			// Day9 已经把该单位降到 Dormant，不应继续占用 Day10 配额。
			// 同时清除旧 BudgetOff，确保它靠近玩家恢复 Significance 后可以立即活动。
			Budget->ApplyBudgetEnabled(true, EAIBudgetAllocationReason::UnderBudget);
			continue;
		}

		FAIBudgetCandidate& Candidate = Candidates.Emplace_GetRef();
		Candidate.Budget = Budget;
		Candidate.Score = Significance != nullptr ? Significance->GetCurrentScore() : 0.0f;
		Candidate.Distance = Significance != nullptr ? Significance->GetDistanceToPlayer() : TNumericLimits<float>::Max();
		Candidate.bProtected = !Budget->CanReduceWork();
	}

	const bool bBudgetEnabled = CVarAIBudgetEnable.GetValueOnGameThread() != 0;
	const int32 OverrideMaxActive = CVarAIBudgetMaxActive.GetValueOnGameThread();
	const int32 MaxActiveCount = OverrideMaxActive >= 0 ? OverrideMaxActive : MaxActiveAITickCount;
	if (!bBudgetEnabled || MaxActiveCount <= 0)
	{
		for (const FAIBudgetCandidate& Candidate : Candidates)
		{
			Candidate.Budget->ApplyBudgetEnabled(true, EAIBudgetAllocationReason::SystemDisabled, true);
		}
		DrawDebugInfo(Candidates.Num(), Candidates.Num(), 0);
		return;
	}

	Candidates.Sort([](const FAIBudgetCandidate& A, const FAIBudgetCandidate& B)
	{
		if (A.bProtected != B.bProtected)
		{
			return A.bProtected;
		}

		if (!FMath::IsNearlyEqual(A.Score, B.Score))
		{
			return A.Score > B.Score;
		}

		return A.Distance < B.Distance;
	});

	const bool bUnderBudget = Candidates.Num() <= MaxActiveCount;
	int32 AllocatedCount = 0;
	int32 ProtectedCount = 0;
	for (const FAIBudgetCandidate& Candidate : Candidates)
	{
		if (Candidate.bProtected)
		{
			++ProtectedCount;
			++AllocatedCount;
			Candidate.Budget->ApplyBudgetEnabled(true, EAIBudgetAllocationReason::Protected);
			continue;
		}

		const bool bEnable = bUnderBudget || AllocatedCount < MaxActiveCount;
		Candidate.Budget->ApplyBudgetEnabled(
			bEnable,
			bUnderBudget ? EAIBudgetAllocationReason::UnderBudget
				: (bEnable ? EAIBudgetAllocationReason::InQuota : EAIBudgetAllocationReason::Overflow));
		if (bEnable)
		{
			++AllocatedCount;
		}
	}

	int32 ActualActiveCount = 0;
	for (const FAIBudgetCandidate& Candidate : Candidates)
	{
		if (Candidate.Budget->IsBudgetEnabled())
		{
			++ActualActiveCount;
		}
	}
	DrawDebugInfo(ActualActiveCount, Candidates.Num(), ProtectedCount);

	if (CVarAIBudgetDebug.GetValueOnGameThread() == 0)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (const FAIBudgetCandidate& Candidate : Candidates)
	{
		const AActionMonsterCharacter* Monster = Candidate.Budget->GetOwnerMonster();
		if (Monster == nullptr)
		{
			continue;
		}

		const FString DebugText = FString::Printf(
			TEXT("Budget=%s | %s | S%.1f"),
			Candidate.Budget->IsBudgetEnabled() ? TEXT("On") : TEXT("Off"),
			*BudgetReasonToString(Candidate.Budget->GetAllocationReason()),
			Candidate.Score);
		DrawDebugString(
			World,
			Monster->GetActorLocation() + FVector(0.0f, 0.0f, 190.0f),
			DebugText,
			nullptr,
			Candidate.Budget->IsBudgetEnabled() ? FColor::Cyan : FColor::Red,
			ReevaluateInterval + 0.1f,
			true);
	}
}

void UAIBudgetSubsystem::DrawDebugInfo(int32 ActiveCount, int32 TotalCount, int32 ProtectedCount) const
{
	if (CVarAIBudgetDebug.GetValueOnGameThread() == 0 || GEngine == nullptr)
	{
		return;
	}

	const int32 OverrideMaxActive = CVarAIBudgetMaxActive.GetValueOnGameThread();
	const int32 MaxActiveCount = OverrideMaxActive >= 0 ? OverrideMaxActive : MaxActiveAITickCount;
	const FString Summary = FString::Printf(
		TEXT("AI Budget: %s | Active=%d Total=%d Protected=%d Max=%d"),
		CVarAIBudgetEnable.GetValueOnGameThread() != 0 ? TEXT("On") : TEXT("Off"),
		ActiveCount,
		TotalCount,
		ProtectedCount,
		MaxActiveCount);
	constexpr uint64 BudgetDebugMessageKey = 0xA1B0D6E7ULL;
	GEngine->AddOnScreenDebugMessage(BudgetDebugMessageKey, ReevaluateInterval + 0.1f, FColor::Cyan, Summary);
}

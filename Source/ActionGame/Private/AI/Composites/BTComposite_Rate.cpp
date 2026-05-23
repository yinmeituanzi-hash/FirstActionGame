#include "AI/Composites/BTComposite_Rate.h"

#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Math/UnrealMathUtility.h"

DEFINE_LOG_CATEGORY_STATIC(LogBTRate, Log, All);

UBTComposite_Rate::UBTComposite_Rate()
{
	NodeName = TEXT("Rate");
}

int32 UBTComposite_Rate::GetNextChildHandler(FBehaviorTreeSearchData& SearchData, int32 PrevChild, EBTNodeResult::Type LastResult) const
{
	// 010 同款语义：只在第一次进入（PrevChild == NotInitialized）时抽取；子节点 Finish 后直接 ReturnToParent，不重抽。
	// 这样保证一次进入只执行一个子节点，不会因为某子节点 Fail 导致循环。
	if (PrevChild != BTSpecialChild::NotInitialized)
	{
		return BTSpecialChild::ReturnToParent;
	}

	const int32 ChosenIdx = PickWeightedChild();

	if (bVerboseDebugLog)
	{
		UE_LOG(
			LogBTRate,
			Log,
			TEXT("BTComposite_Rate '%s': picked child %d (weights=%d, children=%d)"),
			*NodeName,
			ChosenIdx,
			Weights.Num(),
			GetChildrenNum());
	}

	return ChosenIdx;
}

int32 UBTComposite_Rate::PickWeightedChild() const
{
	const int32 ChildCount = GetChildrenNum();
	if (ChildCount <= 0)
	{
		return BTSpecialChild::ReturnToParent;
	}

	// 有效权重 = 当前 i 在 Weights 数组内的子节点权重之和（负数视为 0）。
	int32 TotalWeight = 0;
	for (int32 i = 0; i < ChildCount && i < Weights.Num(); ++i)
	{
		TotalWeight += FMath::Max(0, Weights[i]);
	}

	if (TotalWeight <= 0)
	{
		// 没有任何有效权重的子节点。可能是配置错误（Weights 全 0）。
		// 此时退化到"没得选"返回 ReturnToParent，BT 视为父 Composite 选完一遍空走。
		UE_LOG(
			LogBTRate,
			Warning,
			TEXT("BTComposite_Rate '%s': no valid child to pick (TotalWeight=0). Check Weights config (current size=%d, children=%d)."),
			*NodeName,
			Weights.Num(),
			ChildCount);
		return BTSpecialChild::ReturnToParent;
	}

	// FMath::RandRange(0, TotalWeight - 1)：闭区间，纯整数随机，
	// 用 UE 框架随机系统比 rand() 跨平台一致、可种子化。
	const int32 Roll = FMath::RandRange(0, TotalWeight - 1);
	int32 Accum = 0;
	for (int32 i = 0; i < ChildCount && i < Weights.Num(); ++i)
	{
		const int32 W = FMath::Max(0, Weights[i]);
		Accum += W;
		if (Roll < Accum)
		{
			return i;
		}
	}

	// 兜底：理论上 Roll < TotalWeight 一定会在循环里命中。这里返回最后一个有效子节点。
	for (int32 i = ChildCount - 1; i >= 0; --i)
	{
		if (i < Weights.Num() && Weights[i] > 0)
		{
			return i;
		}
	}
	return BTSpecialChild::ReturnToParent;
}

FString UBTComposite_Rate::GetStaticDescription() const
{
	if (Weights.Num() == 0)
	{
		return TEXT("Rate: <empty weights>");
	}

	int32 Sum = 0;
	for (int32 W : Weights)
	{
		Sum += FMath::Max(0, W);
	}

	FString Result = TEXT("Rate [");
	for (int32 i = 0; i < Weights.Num(); ++i)
	{
		if (i > 0)
		{
			Result += TEXT(", ");
		}
		if (Sum > 0)
		{
			const float Pct = 100.0f * FMath::Max(0, Weights[i]) / static_cast<float>(Sum);
			Result += FString::Printf(TEXT("%.0f%%"), Pct);
		}
		else
		{
			Result += FString::Printf(TEXT("%d"), Weights[i]);
		}
	}
	Result += TEXT("]");

	return Result;
}

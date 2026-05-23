#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BTComposite_Rate.generated.h"

/**
 * Sprint 4-C++ Day 7：按权重抽中一个子节点的复合节点。
 *
 * 行为：
 *  - 第一次进入时按 `Weights` 抽中一个子节点执行。
 *  - 子节点返回结果（Succeeded/Failed）直接传回父节点，**不重抽**——避免无限循环（一个子节点
 *    一直 Fail 会让 Rate 一直重抽下一个，直到所有子都 Fail 才返回）。
 *  - 子节点数大于 Weights 长度时，多出的子节点权重视为 0（不会被抽中）。
 *  - 子节点数小于 Weights 长度时，多出的权重被忽略（不影响概率分布）。
 *
 * 经典用法（怪物攻击分布）：
 *  ```
 *  Composite_Rate [60, 30, 10]
 *    ├─ Sequence (普攻)
 *    ├─ Sequence (重击)
 *    └─ Sequence (Cooldown 10s + 大招)
 *  ```
 *  60% 走普攻，30% 重击，10% 大招（大招 CD 没好时退到普攻）。
 *
 * 参考 010 `UBTComposite_Rate`，我们做了 2 个改进：
 *  1. 用 `FMath::RandRange` 替换 `rand()`：可种子化、跨平台一致、与 UE 框架统一。
 *  2. `GetStaticDescription` 显示可读字符串 `Rate [60%, 30%, 10%]`，
 *     在 BT 视图里不用打开节点就能看清概率分布。
 */
UCLASS()
class ACTIONGAME_API UBTComposite_Rate : public UBTCompositeNode
{
	GENERATED_BODY()

public:
	UBTComposite_Rate();

	/**
	 * 各子节点的权重数组。
	 * 例如：[60, 30, 10] 表示 60%/30%/10%。具体值不需要加起来等于 100，
	 * 内部会按总和归一化，[6, 3, 1] 与 [60, 30, 10] 行为一致。
	 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|Rate")
	TArray<int32> Weights;

	/**
	 * 启用 Verbose 调试 Log，每次抽中时打印权重总和与中签 ChildIdx。
	 * 调 BT 概率分布时打开，平时关掉。
	 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|Rate|Debug")
	bool bVerboseDebugLog = false;

protected:
	virtual int32 GetNextChildHandler(struct FBehaviorTreeSearchData& SearchData, int32 PrevChild, EBTNodeResult::Type LastResult) const override;
	virtual FString GetStaticDescription() const override;

private:
	/** 按权重抽中一个子节点索引。 */
	int32 PickWeightedChild() const;
};

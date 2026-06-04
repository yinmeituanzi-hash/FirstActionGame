#pragma once

#include "CoreMinimal.h"
#include "AIBudgetTypes.generated.h"

/**
 * Budget 分配结果的来源。
 *
 * Enabled / Disabled 是第一版真正执行的状态；Reason 只用于 Debug 和后续扩展。
 * 这样未来升级成多档质量时，不需要重新设计注册和排序接口。
 */
UENUM(BlueprintType)
enum class EAIBudgetAllocationReason : uint8
{
	UnderBudget UMETA(DisplayName = "Under Budget"),
	InQuota UMETA(DisplayName = "In Quota"),
	Protected UMETA(DisplayName = "Protected"),
	Overflow UMETA(DisplayName = "Overflow"),
	SystemDisabled UMETA(DisplayName = "System Disabled"),
	Throttled UMETA(DisplayName = "Throttled")
};

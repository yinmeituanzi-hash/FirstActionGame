#pragma once

#include "CoreMinimal.h"
#include "ActionAITypes.generated.h"

/**
 * 怪物 AI 的粗粒度警戒状态。
 *
 * 这层状态在 BT 之上：BT 根 Selector 只需要按 Idle / Alert / Combat 分三大分支，
 * 不用在每个 Task 里重复判断“有没有目标 / 是否听到声音 / 是否丢失玩家”。
 */
UENUM(BlueprintType)
enum class EAIAlertState : uint8
{
	Idle   UMETA(DisplayName = "Idle / Patrol"),
	Alert  UMETA(DisplayName = "Alert"),
	Combat UMETA(DisplayName = "Combat")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAIAlertStateChangedSignature, EAIAlertState, OldState, EAIAlertState, NewState);

#pragma once

#include "CoreMinimal.h"
#include "AINoiseTypes.generated.h"

/**
 * 噪音种类。Day 5 第一版 Listener 不区分种类一律处理，但保留枚举给后续：
 *  - Combat 噪音半径可能比 Footstep 大一倍
 *  - Footstep 在 Idle/Alert 怪上才生效，Combat 怪自动忽略
 *  - Generic 给玩家自定义触发用（拉拉杆、踢罐子…）
 */
UENUM(BlueprintType)
enum class EAINoiseCategory : uint8
{
	Generic   UMETA(DisplayName = "Generic"),
	Footstep  UMETA(DisplayName = "Footstep"),
	Combat    UMETA(DisplayName = "Combat")
};

/**
 * 一次噪音事件。从 Subsystem 派发给 Listener 时的轻量上下文。
 */
USTRUCT(BlueprintType)
struct FActionAINoiseEvent
{
	GENERATED_BODY()

	/** 噪音发生的世界位置。 */
	UPROPERTY(BlueprintReadWrite, Category = "AI|Noise")
	FVector Location = FVector::ZeroVector;

	/** 噪音传播半径（cm）。Listener.HearingDistance 与本字段取 Min 后再做距离判定。 */
	UPROPERTY(BlueprintReadWrite, Category = "AI|Noise")
	float Loudness = 1000.0f;

	/** 噪音种类。 */
	UPROPERTY(BlueprintReadWrite, Category = "AI|Noise")
	EAINoiseCategory Category = EAINoiseCategory::Generic;

	/** 发出方（可为空，比如环境噪音）。Listener 可用此过滤"自己阵营的声音"。 */
	UPROPERTY(BlueprintReadWrite, Category = "AI|Noise")
	TWeakObjectPtr<AActor> Instigator;
};

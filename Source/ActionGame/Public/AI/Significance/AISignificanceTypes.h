#pragma once

#include "CoreMinimal.h"
#include "Components/SkinnedMeshComponent.h"
#include "AISignificanceTypes.generated.h"

UENUM(BlueprintType)
enum class EAISignificanceLevel : uint8
{
	High UMETA(DisplayName = "High"),
	Medium UMETA(DisplayName = "Medium"),
	Low UMETA(DisplayName = "Low"),
	Dormant UMETA(DisplayName = "Dormant")
};

USTRUCT(BlueprintType)
struct ACTIONGAME_API FAISignificanceLevelConfig
{
	GENERATED_BODY()

	/** BehaviorTreeComponent Tick 间隔。0 表示每帧 Tick。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Significance", meta = (ClampMin = "0.0"))
	float BehaviorTreeTickInterval = 0.0f;

	/** AIController Tick 间隔。Controller 需要 Tick 来平滑朝向，不要轻易在战斗中关掉。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Significance", meta = (ClampMin = "0.0"))
	float ControllerTickInterval = 0.0f;

	/** CharacterMovementComponent Tick 间隔。低频只适合远处非战斗单位。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Significance", meta = (ClampMin = "0.0"))
	float MovementTickInterval = 0.0f;

	/** Mesh Component Tick 间隔。配合 VisibilityBasedAnimTickOption / URO 使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Significance", meta = (ClampMin = "0.0"))
	float MeshTickInterval = 0.0f;

	/** Dormant 级别才建议关闭 BT Tick；Combat / Alert 不应该走到这里。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Significance")
	bool bEnableBehaviorTreeTick = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Significance")
	bool bEnableMovementTick = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Significance")
	bool bEnableMeshTick = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Significance")
	bool bEnableControllerTick = true;

	/** 远处动画打开 URO，近处和战斗中保持稳定表现。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Significance")
	bool bEnableAnimUpdateRateOptimizations = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Significance")
	EVisibilityBasedAnimTickOption VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAISignificanceLevelChangedSignature,
	EAISignificanceLevel, OldLevel,
	EAISignificanceLevel, NewLevel);

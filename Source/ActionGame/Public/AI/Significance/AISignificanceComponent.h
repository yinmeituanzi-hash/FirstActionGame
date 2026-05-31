#pragma once

#include "CoreMinimal.h"
#include "AI/Significance/AISignificanceTypes.h"
#include "Components/ActorComponent.h"
#include "AISignificanceComponent.generated.h"

class AActionMonsterCharacter;
class AActionMonsterAIController;
class UBehaviorTreeComponent;

/**
 * 单体怪物的 AI 重要度组件。
 *
 * 组件只负责“这只怪当前重要度是多少，以及各通道怎么降级”。
 * 全局遍历和采样由 UAISignificanceSubsystem 负责，避免每只怪各自 Tick。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UAISignificanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAISignificanceComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintAssignable, Category = "Action|AI|Significance")
	FAISignificanceLevelChangedSignature OnLevelChanged;

	UFUNCTION(BlueprintPure, Category = "Action|AI|Significance")
	EAISignificanceLevel GetSignificanceLevel() const { return CurrentLevel; }

	UFUNCTION(BlueprintPure, Category = "Action|AI|Significance")
	float GetDistanceToPlayer() const { return DistanceToPlayer; }

	UFUNCTION(BlueprintPure, Category = "Action|AI|Significance")
	float GetCurrentScore() const { return CurrentScore; }

	UFUNCTION(BlueprintPure, Category = "Action|AI|Significance")
	bool IsBehaviorTreeAllowedBySignificance() const { return !bSignificanceBTDisabled; }

	UFUNCTION(BlueprintPure, Category = "Action|AI|Significance")
	bool IsBehaviorTreeAllowedByBudget() const { return !bBudgetBTDisabled; }

	/** Day10 Budget 会调用这里。Day9 先保持 true，但接口提前稳定下来。 */
	UFUNCTION(BlueprintCallable, Category = "Action|AI|Significance")
	void ApplyBudgetEnabled(bool bEnabled);

	void EvaluateAndApply(APawn* PlayerPawn);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance", meta = (AllowPrivateAccess = "true"))
	EAISignificanceLevel CurrentLevel = EAISignificanceLevel::High;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance", meta = (AllowPrivateAccess = "true"))
	float DistanceToPlayer = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance", meta = (AllowPrivateAccess = "true"))
	bool bHasLineOfSightToPlayer = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance", meta = (AllowPrivateAccess = "true"))
	bool bWasRecentlyRendered = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance", meta = (AllowPrivateAccess = "true"))
	float CurrentScore = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance", meta = (AllowPrivateAccess = "true"))
	float DistanceScore = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance", meta = (AllowPrivateAccess = "true"))
	float RenderScore = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Distance", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float HighDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Distance", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MediumDistance = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Distance", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float LowDistance = 5000.0f;

	/** 参考 010：距离越近附加分越高，到该距离外附加分归零。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Score", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float MaxDistanceScoreDistance = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Score", meta = (AllowPrivateAccess = "true"))
	float HighBaseScore = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Score", meta = (AllowPrivateAccess = "true"))
	float MediumBaseScore = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Score", meta = (AllowPrivateAccess = "true"))
	float LowBaseScore = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Score", meta = (AllowPrivateAccess = "true"))
	float HighScoreThreshold = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Score", meta = (AllowPrivateAccess = "true"))
	float MediumScoreThreshold = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Score", meta = (AllowPrivateAccess = "true"))
	float LowScoreThreshold = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Render", meta = (AllowPrivateAccess = "true"))
	bool bUseRenderScore = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Render", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float RenderRecentlyTolerance = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Render", meta = (AllowPrivateAccess = "true"))
	float RenderedBaseScore = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Render", meta = (AllowPrivateAccess = "true"))
	float NotRenderedBaseScore = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Config", meta = (AllowPrivateAccess = "true"))
	FAISignificanceLevelConfig HighConfig;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Config", meta = (AllowPrivateAccess = "true"))
	FAISignificanceLevelConfig MediumConfig;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Config", meta = (AllowPrivateAccess = "true"))
	FAISignificanceLevelConfig LowConfig;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Significance|Config", meta = (AllowPrivateAccess = "true"))
	FAISignificanceLevelConfig DormantConfig;

	bool bSignificanceBTDisabled = false;
	bool bBudgetBTDisabled = false;

	bool bDefaultsCaptured = false;
	bool bDefaultControllerTickEnabled = true;
	bool bDefaultBehaviorTreeTickEnabled = true;
	bool bDefaultMovementTickEnabled = true;
	bool bDefaultMeshTickEnabled = true;
	float DefaultControllerTickInterval = 0.0f;
	float DefaultBehaviorTreeTickInterval = 0.0f;
	float DefaultMovementTickInterval = 0.0f;
	float DefaultMeshTickInterval = 0.0f;
	bool bDefaultEnableAnimURO = false;
	EVisibilityBasedAnimTickOption DefaultVisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	AActionMonsterCharacter* GetOwnerMonster() const;
	AActionMonsterAIController* GetMonsterAIController() const;
	UBehaviorTreeComponent* GetBehaviorTreeComponent() const;
	void CaptureDefaultsIfNeeded();
	EAISignificanceLevel CalculateLevel(APawn* PlayerPawn);
	float CalculateSignificanceScore(AActionMonsterCharacter* Monster, APawn* PlayerPawn);
	float CalculateDistanceScore(float InDistance, float& OutRealDistanceScore) const;
	float CalculateRenderScore(const AActionMonsterCharacter* Monster, float RealDistanceScore);
	EAISignificanceLevel ScoreToLevel(float Score) const;
	EAISignificanceLevel EnforceAlertStateFloor(EAISignificanceLevel InLevel) const;
	void ApplyLevel(EAISignificanceLevel NewLevel);
	void ApplyChannels(const FAISignificanceLevelConfig& Config);
	void ApplyBehaviorTreeTick(const FAISignificanceLevelConfig& Config);
	const FAISignificanceLevelConfig& GetConfigForLevel(EAISignificanceLevel Level) const;
	bool IsAIControlBlocked() const;
	bool ShouldDebugDraw() const;
	void DrawDebugInfo() const;
};
